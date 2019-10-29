#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <asm/atomic.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf-core.h>


/* 参考 uvc_v4l2_do_ioctl */
static int cmos_ov7740_vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	return 0;
}

/* 列举支持哪种格式
 * 参考: uvc_fmts 数组
 */
static int cmos_ov7740_vidioc_enum_fmt_vid_cap(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	return 0;
}

/* 返回当前所使用的格式 */
static int cmos_ov7740_vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	return 0;
}

/* 测试驱动程序是否支持某种格式, 强制设置该格式 
 * 参考: uvc_v4l2_try_format
 *       myvivi_vidioc_try_fmt_vid_cap
 */
static int cmos_ov7740_vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	return 0;
}

/* 参考 myvivi_vidioc_s_fmt_vid_cap */
static int cmos_ov7740_vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	return 0;
}

//申请缓存区
static int cmos_ov7740_vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *p)
{
	return 0;
}

/* 启动传输 
 * 参考: uvc_video_enable(video, 1):
 *           uvc_commit_video
 *           uvc_init_video
 */
static int cmos_ov7740_vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	return 0;
}

/* 停止 
 * 参考 : uvc_video_enable(video, 0)
 */
static int cmos_ov7740_vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type t)
{
	return 0;
}


static const struct v4l2_ioctl_ops cmos_ov7740_ioctl_ops = {
        // 表示它是一个摄像头设备
        .vidioc_querycap      = cmos_ov7740_vidioc_querycap,

        /* 用于列举、获得、测试、设置摄像头的数据的格式 */
        .vidioc_enum_fmt_vid_cap  = cmos_ov7740_vidioc_enum_fmt_vid_cap,
        .vidioc_g_fmt_vid_cap     = cmos_ov7740_vidioc_g_fmt_vid_cap,
        .vidioc_try_fmt_vid_cap   = cmos_ov7740_vidioc_try_fmt_vid_cap,
        .vidioc_s_fmt_vid_cap     = cmos_ov7740_vidioc_s_fmt_vid_cap,
        
        /* 缓冲区操作: 申请/查询/放入队列/取出队列 */
        .vidioc_reqbufs       = cmos_ov7740_vidioc_reqbufs,

	/* 说明: 因为我们是通过读的方式来获得摄像头数据,因此查询/放入队列/取出队列这些操作函数将不在需要 */
#if 0
        .vidioc_querybuf      = myuvc_vidioc_querybuf,
        .vidioc_qbuf          = myuvc_vidioc_qbuf,
        .vidioc_dqbuf         = myuvc_vidioc_dqbuf,
#endif

        // 启动/停止
        .vidioc_streamon      = cmos_ov7740_vidioc_streamon,
        .vidioc_streamoff     = cmos_ov7740_vidioc_streamoff,   
};



/* 打开摄像头设备 */
static int cmos_ov7740_open(struct file *file)
{
	return 0;
}

/* 关闭摄像头设备 */
static int cmos_ov7740_close(struct file *file)
{
    
	return 0;
}

/* 应用程序通过读的方式读取摄像头的数据 */
static ssize_t cmos_ov7740_read(struct file *filep, char __user *buf, size_t count, loff_t *pos)
{
	return 0;
}


/* 3.1. 分配、设置一个v4l2_file_operations结构体 
 * 成员为video_device设备的操作函数
 */
static const struct v4l2_file_operations cmos_ov7740_fops = {
	.owner			= THIS_MODULE,
	.open       		= cmos_ov7740_open,
	.release    		= cmos_ov7740_close,
	.unlocked_ioctl     = video_ioctl2,
	.read			= cmos_ov7740_read,
};


/* 2.1. 分配、设置一个video_device结构体 
 * 
 */
static struct video_device cmos_ov7740_vdev = {
	.fops		= &cmos_ov7740_fops,
	.ioctl_ops		= &cmos_ov7740_ioctl_ops,
	.release		= cmos_ov7740_release,
	.name		= "cmos_ov7740",
};

//当drv name 与dev name相同时会执行probe函数
//需要在probe函数中注册video设备
static int __devinit cmos_ov7740_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	/* 2.2.注册设备 */
    video_register_device(&cmos_ov7740_vdev, VFL_TYPE_GRABBER, -1);

    return 0;

}

//当rmmod此设备驱动时会执行此函数
//remove 函数中移除video设备
static int __devexit cmos_ov7740_remove(struct i2c_client *client)
{   
   printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

   video_unregister_device(&cmos_ov7740_vdev);
   return 0; 

}


static const struct i2c_device_id cmos_ov7740_id_table[] = {
	{ "cmos_ov7740", 0 },
	{}
};
	

/* 1.1. 分配、设置一个i2c_driver */
static struct i2c_driver cmos_ov7740_driver =
{
	.driver = {
		.name	= "cmos_ov7740",
		.owner	= THIS_MODULE,
	},
	.probe		= cmos_ov7740_probe, 
	.remove 	= __devexit_p ( cmos_ov7740_remove ),
	.id_table	= cmos_ov7740_id_table,
};

static int cmos_ov7740_drv_init ( void )
{

	/* 1.2.注册驱动 */
	i2c_add_driver ( &cmos_ov7740_driver );

}


static void cmos_ov7740_drv_exit ( void )
{
	//退出函数里删除驱动
	i2c_del_driver ( &cmos_ov7740_driver );
}

module_init ( cmos_ov7740_drv_init );
module_exit ( cmos_ov7740_drv_exit );

MODULE_LICENSE ( "GPL" );

