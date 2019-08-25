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



int major; //���豸��
static struct class* Buttonv_class; //�豸�ڵ�
static struct class_device*	Button_class_dev;

volatile unsigned long* gpfcon;
volatile unsigned long* gpfdat;

volatile unsigned long* gpgcon;
volatile unsigned long* gpgdat;


static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

/* �ж��¼���־, �жϷ����������1��read����������0 */
static volatile int ev_press = 0;

static struct fasync_struct *button_async;

static DECLARE_MUTEX(button_lock);

static struct timer_list buttons_timer; //��ʼ����ʱ��


typedef struct Pin_Desc
{
	unsigned int pin;
	unsigned int key_val;

} T_Pin_Desc,*PT_Pin_Desc;

//����
static T_Pin_Desc pins_desc[4] =
{
	{S3C2410_GPF0,0x01},
	{S3C2410_GPF2,0x02},
	{S3C2410_GPG3,0x03},
	{S3C2410_GPG11,0x04},
};

static unsigned char key_val = 0;

static PT_Pin_Desc irq_dev_id ; //


//�жϴ������� 
static irqreturn_t Button_handler ( int irq, void* dev_id )
{
	irq_dev_id = (PT_Pin_Desc )dev_id;
    //printk("%s",__FUNCTION__ ); ��ͨ����ӡ�۲� �жϵ�ִ�����
	
    //ˢ�¶�ʱ����ʱʱ��
    mod_timer(&buttons_timer, jiffies+HZ/100); //jiffies��λ��Hz ���HZ�ĵ�λΪ100 jiffies+1 ��������10ms

	
	return IRQ_RETVAL ( IRQ_HANDLED );

}

//������ʼ��
static int Button_open ( struct inode* inode, struct file* file )
{
    //���ʹ�÷�������ʽ���˱����� ���Ի�ȡ�ź��� ���ʧ�� ֱ�ӷ�������æ
    if (file->f_flags & O_NONBLOCK)
	{
		if (down_trylock(&button_lock))
			return -EBUSY;
	}
	else
	{
		down(&button_lock); //ʹ��������ʽ�򿪱�����            ���Ȼ�ȡ�ź��� �����ȡ������������� �ȴ��������̹ر�
	}


	//�����ж�       �жϺ� �������� ������ʽ �ж��� �豸id
	request_irq ( IRQ_EINT0, Button_handler,IRQT_BOTHEDGE, "button1", &pins_desc[0] );
	request_irq ( IRQ_EINT2, Button_handler,IRQT_BOTHEDGE, "button2", &pins_desc[1] );
	request_irq ( IRQ_EINT11, Button_handler,IRQT_BOTHEDGE, "button3", &pins_desc[2] );
	request_irq ( IRQ_EINT19, Button_handler,IRQT_BOTHEDGE, "button4", &pins_desc[3] );
	return 0;
}


static ssize_t Read_Button ( struct file* file, char __user* buf, size_t size, loff_t* ppos )
{

    //�������ֵ��Ϊ1 ���ز�����Ч
	if (size != 1)
		return -EINVAL;

    if (file->f_flags & O_NONBLOCK)
	{
      if(!ev_press)
	    return -EAGAIN; //����Ƿ�������ʽ��ȡ û������ʱֱ�ӷ���
	
    }
	else
	{
		/* ���û�а�������, ���� ���ev_press����1 �˳�����*/
		wait_event_interruptible(button_waitq, ev_press);
	}
   
	//�������ݸ��û�
	copy_to_user ( buf, &key_val, 1 );
	ev_press = 0;

	return 1; //�ɹ���ȡ���ֽ��� �Ǹ�
}


int Button_irq_release (struct inode *inode , struct file *file)
{
    //�ͷ��ж�
   	free_irq(IRQ_EINT0, &pins_desc[0]);
	free_irq(IRQ_EINT2, &pins_desc[1]);
	free_irq(IRQ_EINT11, &pins_desc[2]);
	free_irq(IRQ_EINT19, &pins_desc[3]);
	 
	up(&button_lock);

	return 0;
}

//�ں��е�do_poll�ᶨʱ����ָ�������е�poll���� �����������ķ���ֵ��0 �ỽ��ע��Ľ���
unsigned int Button_poll (struct file *file, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
	poll_wait(file, &button_waitq, wait); // ������������ ����ǰ���̹ҵ�ָ�������� 

	//����ж����а������� ev_press���ᱻ����Ϊ1 ����maskΪ��0ֵ  
	if (ev_press) 
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

//ִ��fcntl �ı�fasync��ǻ���ô˺���
static int Button_drv_fasync (int fd, struct file *filp, int on)
{
	printk("driver: Button_drv_fasync\n");
	return fasync_helper (fd, filp, on, &button_async);
}


static struct file_operations Button_drv_fops =
{
	.owner  =   THIS_MODULE,    /* ����һ���꣬�������ģ��ʱ�Զ�������__this_module���� */
	.open   =   Button_open,    //Ӧ�ó���openʱ����� 
	.read	=	Read_Button,    //Ӧ�ó���readʱ�����
	.release =  Button_irq_release, //Ӧ�ó���closeʱ����� 
	.poll   =   Button_poll,   
	.fasync	 =  Button_drv_fasync,
};

static void Button_timer_handler(unsigned long data)
{
	PT_Pin_Desc pin_desc = irq_dev_id; //
	unsigned int pinval;
	
	pinval = s3c2410_gpio_getpin ( pin_desc->pin ); //����2410�⺯�� ��ȡio��״̬
	
	if ( pinval ) //
	{
	   key_val = pin_desc->key_val | 0x80;	
	}
	else
	{  
	   key_val = pin_desc->key_val ; 
	}


	ev_press = 1;
	wake_up_interruptible(&button_waitq);	/* �������ߵĽ��� */

	kill_fasync (&button_async, SIGIO, POLL_IN); //���а�������ʱ֪ͨӦ�ó���

}


//insmod��������ʱִ��
static int Button_init ( void )
{
    /*��ʼ����ʱ��*/
    init_timer(&buttons_timer);
	//���ö�ʱ����ʱ����
	buttons_timer.function = Button_timer_handler;
	//���Ӷ�ʱ��
	add_timer(&buttons_timer);



	//����ֵΪϵͳ��������豸��
	major = register_chrdev ( 0, "Button_drv", &Button_drv_fops ); //���豸�� 0-Ϊ�Զ�����   �豸��  file_operations�ṹ��

	//�Զ������豸�ڵ� /dev/buttons
	Buttonv_class = class_create ( THIS_MODULE, "Button_drv" );
	Button_class_dev = class_device_create ( Buttonv_class, NULL, MKDEV ( major, 0 ), NULL, "buttons" ); /* /dev/buttons */

	//���������ַ 0x56000050��ΪĿ�굥��Ĵ���������ַ 16λ����
	gpfcon = ( volatile unsigned long* ) ioremap ( 0x56000050, 16 );
	gpfdat = gpfcon + 1;

	gpgcon = ( volatile unsigned long* ) ioremap ( 0x56000060, 16 );
	gpgdat = gpgcon + 1;

	return 0;

}


static void Button_exit ( void )
{
	unregister_chrdev ( major, "Button_drv" ); //ȡ��ע��

	//����豸�ڵ�
	class_device_unregister ( Button_class_dev );
	class_destroy ( Buttonv_class );

	iounmap ( gpfcon );
	iounmap ( gpgcon );

	//return 0;
}



//ע���ʼ���Լ��˳���������
module_init ( Button_init );
module_exit ( Button_exit );


MODULE_LICENSE ( "GPL" ); //ģ�������֤����




