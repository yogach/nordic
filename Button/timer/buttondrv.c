#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/interrupt.h>
#include <linux/poll.h>



int major; //主设备号
static struct class* Buttonv_class; //设备节点
static struct class_device*	Button_class_dev;

volatile unsigned long* gpfcon;
volatile unsigned long* gpfdat;

volatile unsigned long* gpgcon;
volatile unsigned long* gpgdat;


static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

/* 中断事件标志, 中断服务程序将它置1，read函数将它清0 */
static volatile int ev_press = 0;

static struct fasync_struct *button_async;

static DECLARE_MUTEX(button_lock);

static struct timer_list buttons_timer; //初始化定时器


typedef struct Pin_Desc
{
	unsigned int pin;
	unsigned int key_val;

} T_Pin_Desc,*PT_Pin_Desc;

//用于
static T_Pin_Desc pins_desc[4] =
{
	{S3C2410_GPF0,0x01},
	{S3C2410_GPF2,0x02},
	{S3C2410_GPG3,0x03},
	{S3C2410_GPG11,0x04},
};

static unsigned char key_val = 0;

static PT_Pin_Desc irq_dev_id ; //


//中断处理函数 
static irqreturn_t Button_handler ( int irq, void* dev_id )
{
	irq_dev_id = (PT_Pin_Desc )dev_id;
    //printk("%s",__FUNCTION__ ); 可通过打印观察 中断的执行情况
	
    //刷新定时器超时时间
    mod_timer(&buttons_timer, jiffies+HZ/100); //jiffies单位是Hz 如果HZ的单位为100 jiffies+1 等于增加10ms

	
	return IRQ_RETVAL ( IRQ_HANDLED );

}

//按键初始化
static int Button_open ( struct inode* inode, struct file* file )
{
    //如果使用非阻塞方式打开了本驱动 尝试获取信号量 如果失败 直接返回驱动忙
    if (file->f_flags & O_NONBLOCK)
	{
		if (down_trylock(&button_lock))
			return -EBUSY;
	}
	else
	{
		down(&button_lock); //使用阻塞方式打开本驱动            会先获取信号量 如果获取不到则进入休眠 等待其他进程关闭
	}


	//申请中断       中断号 处理函数 触发方式 中断名 设备id
	request_irq ( IRQ_EINT0, Button_handler,IRQT_BOTHEDGE, "button1", &pins_desc[0] );
	request_irq ( IRQ_EINT2, Button_handler,IRQT_BOTHEDGE, "button2", &pins_desc[1] );
	request_irq ( IRQ_EINT11, Button_handler,IRQT_BOTHEDGE, "button3", &pins_desc[2] );
	request_irq ( IRQ_EINT19, Button_handler,IRQT_BOTHEDGE, "button4", &pins_desc[3] );
	return 0;
}


static ssize_t Read_Button ( struct file* file, char __user* buf, size_t size, loff_t* ppos )
{

    //如果传入值不为1 返回参数无效
	if (size != 1)
		return -EINVAL;

    if (file->f_flags & O_NONBLOCK)
	{
      if(!ev_press)
	    return -EAGAIN; //如果是非阻塞方式读取 没有数据时直接返回
	
    }
	else
	{
		/* 如果没有按键动作, 休眠 如果ev_press等于1 退出休眠*/
		wait_event_interruptible(button_waitq, ev_press);
	}
   
	//拷贝数据给用户
	copy_to_user ( buf, &key_val, 1 );
	ev_press = 0;

	return 1; //成功读取的字节数 非负
}


int Button_irq_release (struct inode *inode , struct file *file)
{
    //释放中断
   	free_irq(IRQ_EINT0, &pins_desc[0]);
	free_irq(IRQ_EINT2, &pins_desc[1]);
	free_irq(IRQ_EINT11, &pins_desc[2]);
	free_irq(IRQ_EINT19, &pins_desc[3]);
	 
	up(&button_lock);

	return 0;
}

//内核中的do_poll会定时调用指定队列中的poll函数 如果驱动程序的返回值非0 会唤醒注册的进程
unsigned int Button_poll (struct file *file, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
	poll_wait(file, &button_waitq, wait); // 不会立即休眠 将当前进程挂到指定队列中 

	//如果中断中有按键按下 ev_press将会被设置为1 设置mask为非0值  
	if (ev_press) 
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

//执行fcntl 改变fasync标记会调用此函数
static int Button_drv_fasync (int fd, struct file *filp, int on)
{
	printk("driver: Button_drv_fasync\n");
	return fasync_helper (fd, filp, on, &button_async);
}


static struct file_operations Button_drv_fops =
{
	.owner  =   THIS_MODULE,    /* 这是一个宏，推向编译模块时自动创建的__this_module变量 */
	.open   =   Button_open,    //应用程序open时会调用 
	.read	=	Read_Button,    //应用程序read时会调用
	.release =  Button_irq_release, //应用程序close时会调用 
	.poll   =   Button_poll,   
	.fasync	 =  Button_drv_fasync,
};

static void Button_timer_handler(unsigned long data)
{
	PT_Pin_Desc pin_desc = irq_dev_id; //
	unsigned int pinval;
	
	pinval = s3c2410_gpio_getpin ( pin_desc->pin ); //调用2410库函数 获取io口状态
	
	if ( pinval ) //
	{
	   key_val = pin_desc->key_val | 0x80;	
	}
	else
	{  
	   key_val = pin_desc->key_val ; 
	}


	ev_press = 1;
	wake_up_interruptible(&button_waitq);	/* 唤醒休眠的进程 */

	kill_fasync (&button_async, SIGIO, POLL_IN); //当有按键按下时通知应用程序

}


//insmod驱动程序时执行
static int Button_init ( void )
{
    /*初始化定时器*/
    init_timer(&buttons_timer);
	//设置定时器超时函数
	buttons_timer.function = Button_timer_handler;
	//添加定时器
	add_timer(&buttons_timer);



	//返回值为系统分配的主设备号
	major = register_chrdev ( 0, "Button_drv", &Button_drv_fops ); //主设备号 0-为自动分配   设备名  file_operations结构体

	//自动生成设备节点 /dev/buttons
	Buttonv_class = class_create ( THIS_MODULE, "Button_drv" );
	Button_class_dev = class_device_create ( Buttonv_class, NULL, MKDEV ( major, 0 ), NULL, "buttons" ); /* /dev/buttons */

	//设置虚拟地址 0x56000050此为目标单板寄存器物理地址 16位长度
	gpfcon = ( volatile unsigned long* ) ioremap ( 0x56000050, 16 );
	gpfdat = gpfcon + 1;

	gpgcon = ( volatile unsigned long* ) ioremap ( 0x56000060, 16 );
	gpgdat = gpgcon + 1;

	return 0;

}


static void Button_exit ( void )
{
	unregister_chrdev ( major, "Button_drv" ); //取消注册

	//清除设备节点
	class_device_unregister ( Button_class_dev );
	class_destroy ( Buttonv_class );

	iounmap ( gpfcon );
	iounmap ( gpgcon );

	//return 0;
}



//注册初始化以及退出驱动函数
module_init ( Button_init );
module_exit ( Button_exit );


MODULE_LICENSE ( "GPL" ); //模块的许可证声明





