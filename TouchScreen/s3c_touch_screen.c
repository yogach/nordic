#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/plat-s3c24xx/ts.h>

#include <asm/arch/regs-adc.h>
#include <asm/arch/regs-gpio.h>





struct s3c_ts_regs
{
	unsigned long adccon;
	unsigned long adctsc;
	unsigned long adcdly;
	unsigned long adcdat0;
	unsigned long adcdat1;
	unsigned long adcupdn;


};

static struct input_dev* s3c_ts_dev;
static volatile struct s3c_ts_regs* s3c_ts_regs;

static void enter_wait_pen_down_mode ( void )
{
	//设置中断产生方式为等待按下
	s3c_ts_regs->adctsc = 0xd3;
}


static void enter_wait_pen_up_mode ( void )
{
	//设置中断产生方式为等待松开
	s3c_ts_regs->adctsc = 0x1d3;
}

static void enter_measure_xy_mode(void)
{
	s3c_ts_regs->adctsc =  (1<<3)|(1<<2);

}

static void start_adc(void)
{
     //启动定时器
     s3c_ts_regs->adccon |=  (1<<0);

}

static irqreturn_t pen_down_up_irq ( int irq, void* dev_id )
{
    /*
	*  ADCDAT0 Bit Description Initial State
	*	UPDOWN [15] Up or Down state of stylus at waiting for interrupt mode. 
	*	0 = Stylus down state. 
	*	1 = Stylus up state.    
    */

	if ( s3c_ts_regs->adcdat0 & ( 1<<15 ) )
	{
       printk("pen up\r\n");
	   enter_wait_pen_down_mode();
	
	}
	else
	{
		//printk("pen down\r\n");
		//enter_wait_pen_up_mode();
		enter_measure_xy_mode();
        start_adc();
		
	}

	return IRQ_HANDLED;
}

static irqreturn_t adc_irq ( int irq, void* dev_id )
{
    static int cnt = 0;
    printk("adc_irq  cnt: %d x:%d y:%d \r\n",cnt++,s3c_ts_regs->adcdat0 & 0x3ff,s3c_ts_regs->adcdat1 &0x3ff);
	enter_wait_pen_up_mode();
	return IRQ_HANDLED;
}

static int s3c_ts_init ( void )
{
    struct clk* clk;

	/* 1. 分配一个input_dev结构体 */
	s3c_ts_dev = input_allocate_device();

	/* 2. 设置 */
	/* 2.1 能产生按键事件         绝对位移(触摸屏)事件 */
	set_bit ( EV_KEY, s3c_ts_dev->evbit );
	set_bit ( EV_ABS, s3c_ts_dev->evbit );

	/* 2.2 能产生这类事件里的哪些事件 */
	set_bit ( BTN_TOUCH, s3c_ts_dev->keybit );

	input_set_abs_params ( s3c_ts_dev, ABS_X, 0, 0x3FF, 0, 0 ); //
	input_set_abs_params ( s3c_ts_dev, ABS_Y, 0, 0x3FF, 0, 0 ); //
	input_set_abs_params ( s3c_ts_dev, ABS_PRESSURE, 0, 1, 0, 0 ); //压力事件


	/* 3. 注册input结构体 */
	input_register_device ( s3c_ts_dev );

	/* 4. 硬件相关的操作 */
	/* 4.1 使能时钟(CLKCON[15]) 打开adc功能*/
	clk = clk_get ( NULL, "adc" );
	clk_enable ( clk );


	/* 4.2 设置S3C2440的ADC/TS寄存器 */
	s3c_ts_regs = ioremap ( 0x58000000, sizeof ( struct s3c_ts_regs ) ); //得到寄存器虚拟地址

	/* bit[14]  : 1-A/D converter prescaler enable 使能ADC转化时钟预分频
	 * bit[13:6]: A/D converter prescaler value,
	 *            49, ADCCLK=PCLK/(49+1)=50MHz/(49+1)=1MHz //设置adc时钟为1MHz
	 * bit[0]: A/D conversion starts by enable. 先设为0
	 */
	s3c_ts_regs->adccon = ( 1<<14 ) | ( 49<<6 );

	request_irq ( IRQ_TC, pen_down_up_irq, IRQF_SAMPLE_RANDOM, "ts_pen", NULL );//申请触摸屏中断
 	request_irq(IRQ_ADC, adc_irq, IRQF_SAMPLE_RANDOM, "adc", NULL);//申请ADC中断

	enter_wait_pen_down_mode();

	return 0;


}


static void s3c_ts_exit ( void )
{
	free_irq ( IRQ_TC,NULL );
	iounmap ( s3c_ts_regs );

	input_unregister_device ( s3c_ts_dev );
	input_free_device ( s3c_ts_dev );


}

module_init ( s3c_ts_init );
module_exit ( s3c_ts_exit );


MODULE_LICENSE ( "GPL" );

