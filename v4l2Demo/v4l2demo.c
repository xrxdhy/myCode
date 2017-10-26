#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#define MAX_BUFFERS 32

static int max_buffers = 8;
static int max_openers = 10;

struct v4l2l_buffer {
	struct v4l2_buffer buffer;
	struct list_head list_head;
	int use_count;
};

struct v4l2_demo_device {
	struct v4l2_device v4l2_dev;
	struct v4l2_ctrl_handler ctrl_handler;
	struct video_device *vdev;
	/* pixel and stream format */
	struct v4l2_pix_format pix_format;
	struct v4l2_captureparm capture_param;
	unsigned long frame_jiffies;

	/* ctrls */
	int keep_format; /* CID_KEEP_FORMAT; stay ready_for_capture even when all
			    openers close() the device */
	int sustain_framerate; /* CID_SUSTAIN_FRAMERATE; duplicate frames to maintain
				  (close to) nominal framerate */

	/* buffers stuff */
	u8 *image;         /* pointer to actual buffers data */
	unsigned long int imagesize;  /* size of buffers data */
	int buffers_number;  /* should not be big, 4 is a good choice */
	struct v4l2l_buffer buffers[MAX_BUFFERS];	/* inner driver buffers */
	int used_buffers; /* number of the actually used buffers */
	int max_openers;  /* how many times can this device be opened */

	int write_position; /* number of last written frame + 1 */
	struct list_head outbufs_list; /* buffers in output DQBUF order */
	int bufpos2index[MAX_BUFFERS]; /* mapping of (read/write_position % used_buffers)
					* to inner buffer index */
	long buffer_size;

	/* sustain_framerate stuff */
	struct timer_list sustain_timer;
	unsigned int reread_count;

	/* timeout stuff */
	unsigned long timeout_jiffies; /* CID_TIMEOUT; 0 means disabled */
	int timeout_image_io; /* CID_TIMEOUT_IMAGE_IO; next opener will
			       * read/write to timeout_image */
	u8 *timeout_image; /* copy of it will be captured when timeout passes */
	struct v4l2l_buffer timeout_image_buffer;
	struct timer_list timeout_timer;
	int timeout_happened;

	/* sync stuff */
	atomic_t open_count;


	int ready_for_capture;/* set to true when at least one writer opened
			       * device and negotiated format */
	int ready_for_output; /* set to true when no writer is currently attached
			       * this differs slightly from !ready_for_capture,
			       * e.g. when using fallback images */
	int announce_all_caps;/* set to false, if device caps (OUTPUT/CAPTURE)
			       * should only be announced if the resp. "ready"
			       * flag is set; default=TRUE */

	wait_queue_head_t read_event;
	spinlock_t lock;
};

struct v4l2_demo_device *dev;


static void free_devices(void)
{
	;
}

static int set_timeperframe(struct v4l2_demo_device *dev,
		struct v4l2_fract *tpf)
{
	if((tpf->denominator < 1) || (tpf->numerator < 1)) {
	  return -EINVAL;
	}
	dev->capture_param.timeperframe = *tpf;
	dev->frame_jiffies = max(1UL,
		msecs_to_jiffies(1000) * tpf->numerator / tpf->denominator);
	return 0;
}

/* init default capture parameters, only fps may be changed in future */
static void init_capture_param(struct v4l2_captureparm *capture_param)
{
	capture_param->capability               = 0;
	capture_param->capturemode              = 0;
	capture_param->extendedmode             = 0;
	capture_param->readbuffers              = max_buffers;
	capture_param->timeperframe.numerator   = 1;
	capture_param->timeperframe.denominator = 30;
}

/* frees buffers, if already allocated */
static int free_buffers(struct v4l2_demo_device *dev)
{
	printk("freeing image@%p for dev:%p\n", dev ? dev->image : NULL, dev);
	if (dev->image) {
		vfree(dev->image);
		dev->image = NULL;
	}
	if (dev->timeout_image) {
		vfree(dev->timeout_image);
		dev->timeout_image = NULL;
	}
	dev->imagesize = 0;

	return 0;
}
/* frees buffers, if they are no longer needed */
static void try_free_buffers(struct v4l2_demo_device *dev)
{
	if (0 == dev->open_count.counter && !dev->keep_format) {
		free_buffers(dev);
		dev->ready_for_capture = 0;
		dev->buffer_size = 0;
		dev->write_position = 0;
	}
}

static int allocate_timeout_image(struct v4l2_demo_device *dev)
{
	if (dev->buffer_size <= 0)
		return -EINVAL;

	if (dev->timeout_image == NULL) {
		dev->timeout_image = vzalloc(dev->buffer_size);
		if (dev->timeout_image == NULL)
			return -ENOMEM;
	}
	return 0;
}

/* init inner buffers, they are capture mode and flags are set as
 * for capture mod buffers */
static void init_buffers(struct v4l2_demo_device *dev)
{
	int i;
	int buffer_size;
	int bytesused;

	buffer_size = dev->buffer_size;
	bytesused = dev->pix_format.sizeimage;

	for (i = 0; i < dev->buffers_number; ++i) {
		struct v4l2_buffer *b = &dev->buffers[i].buffer;
		b->index             = i;
		b->bytesused         = bytesused;
		b->length            = buffer_size;
		b->field             = V4L2_FIELD_NONE;
		b->flags             = 0;
		b->m.offset          = i * buffer_size;
		b->memory            = V4L2_MEMORY_MMAP;
		b->sequence          = 0;
		b->timestamp.tv_sec  = 0;
		b->timestamp.tv_usec = 0;
		b->type              = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		do_gettimeofday(&b->timestamp);
	}
	dev->timeout_image_buffer = dev->buffers[0];
	dev->timeout_image_buffer.buffer.m.offset = MAX_BUFFERS * buffer_size;
}

/* allocates buffers, if buffer_size is set */
static int allocate_buffers(struct v4l2_demo_device *dev)
{
	/* vfree on close file operation in case no open handles left */
	if (0 == dev->buffer_size)
		return -EINVAL;

	if (dev->image) {
		printk("allocating buffers again: %ld %ld\n",
			dev->buffer_size * dev->buffers_number, dev->imagesize);
		/* FIXME: prevent double allocation more intelligently! */
		if (dev->buffer_size * dev->buffers_number == dev->imagesize)
			return 0;

		/* if there is only one writer, no problem should occur */
		if (dev->open_count.counter == 1)
			free_buffers(dev);
		else
			return -EINVAL;
	}

	dev->imagesize = dev->buffer_size * dev->buffers_number;

	printk("allocating %ld = %ldx%d\n", dev->imagesize, dev->buffer_size, dev->buffers_number);

	dev->image = vmalloc(dev->imagesize);
	if (dev->timeout_jiffies > 0)
		allocate_timeout_image(dev);

	if (dev->image == NULL)
		return -ENOMEM;
	printk("vmallocated %ld bytes\n", dev->imagesize);
	init_buffers(dev);
	return 0;
}

static const struct v4l2_file_operations v4l2_demo_fops ={

};

static const struct v4l2_ioctl_ops v4l2_demo_ioctl_ops = {

};

static int __init v4l2demo_init_module(void)
{
	int ret;
	//效果等同于先是用 kmalloc() 申请空间 , 然后用 memset() 来初始化
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if(dev == NULL)
	{
		free_devices();
		return -ENOMEM;
	}

	struct v4l2_ctrl_handler *hdl = &dev->ctrl_handler;
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "v4l2_dev of v4l2demo");
	dev->vdev = video_device_alloc();
	if(dev->vdev == NULL)
	{
		ret = -ENOMEM;
		goto error;
	}

	//video device init
	snprintf(dev->vdev->name,sizeof(dev->vdev->name),"v4l2demo");
	/*VFL_TYPE_GRABBER 表明是一个图像采集设备–包括摄像头、调谐器,诸如此类。*/
	dev->vdev->vfl_type     = VFL_TYPE_GRABBER;
	dev->vdev->fops         = &v4l2_demo_fops;
	dev->vdev->ioctl_ops    = &v4l2_demo_ioctl_ops;
	dev->vdev->release      = &video_device_release;
	dev->vdev->minor        = -1;
	dev->vdev->v4l2_dev = &dev->v4l2_dev;

	init_capture_param(&dev->capture_param);
	set_timeperframe(dev, &dev->capture_param.timeperframe);
	dev->keep_format = 0;
	dev->sustain_framerate = 0;
	dev->buffers_number = max_buffers;
	dev->used_buffers = max_buffers;
	dev->max_openers = max_openers;
	dev->write_position = 0;

	spin_lock_init(&dev->lock);

	INIT_LIST_HEAD(&dev->outbufs_list);
	if (list_empty(&dev->outbufs_list)) {
		int i;

		for (i = 0; i < dev->used_buffers; ++i)
			list_add_tail(&dev->buffers[i].list_head, &dev->outbufs_list);
	}
	memset(dev->bufpos2index, 0, sizeof(dev->bufpos2index));
	atomic_set(&dev->open_count, 0);
	dev->ready_for_capture = 0;
	dev->ready_for_output  = 1;
	//dev->announce_all_caps = (!exclusive_caps[nr]);

	dev->buffer_size = 0;
	dev->image = NULL;
	dev->imagesize = 0;
	//setup_timer(&dev->sustain_timer, sustain_timer_clb, nr);
	dev->reread_count = 0;
	//setup_timer(&dev->timeout_timer, timeout_timer_clb, nr);
	dev->timeout_jiffies = 0;
	dev->timeout_image = NULL;
	dev->timeout_happened = 0;

	ret = v4l2_ctrl_handler_init(hdl, 1);
	if(ret)
		goto error;
	// v4l2_ctrl_new_custom(hdl, &v4l2demo_ctrl_keepformat, NULL);
	// v4l2_ctrl_new_custom(hdl, &v4l2demo_ctrl_sustainframerate, NULL);
	// v4l2_ctrl_new_custom(hdl, &v4l2demo_ctrl_timeout, NULL);
	// v4l2_ctrl_new_custom(hdl, &v4l2demo_ctrl_timeoutimageio, NULL);
	if(hdl->error){
		ret = hdl->error;
		goto error;
	}
	dev->v4l2_dev.ctrl_handler = hdl;

	/* Set initial format */
	dev->pix_format.width = 0; /* V4L2demo_SIZE_DEFAULT_WIDTH; */
	dev->pix_format.height = 0; /* V4L2demo_SIZE_DEFAULT_HEIGHT; */
	//dev->pix_format.pixelformat = formats[0].fourcc;
	dev->pix_format.colorspace = V4L2_COLORSPACE_SRGB; /* do we need to set this ? */
	dev->pix_format.field = V4L2_FIELD_NONE;

	dev->buffer_size = PAGE_ALIGN(dev->pix_format.sizeimage);
	printk("buffer_size = %ld (=%d)\n", dev->buffer_size, dev->pix_format.sizeimage);
	allocate_buffers(dev);
	init_waitqueue_head(&dev->read_event);
	return 0;

error:
	v4l2_device_unregister(&dev->v4l2_dev);
	kfree(dev->vdev);
	return ret;
}

static void v4l2demo_cleanup_module(void)
{
	/* unregister the device -> it deletes /dev/video* */
	free_devices();
	printk("module removed\n");
}

int __init init_module(void)
{
	return v4l2demo_init_module();
}

void __exit cleanup_module(void)
{
	return v4l2demo_cleanup_module();
}