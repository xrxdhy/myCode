#include <linux/videodev2.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#define MAX_BUFFERS 32

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


static int __init v4l2demo_init_module(void)
{
	//效果等同于先是用 kmalloc() 申请空间 , 然后用 memset() 来初始化
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if(dev == NULL)
	{
		free_devices();
		return -ENOMEM;
	}

	//struct v4l2_ctrl_handler *hdl = dev->ctrl_handler;
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "v4l2demo");

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