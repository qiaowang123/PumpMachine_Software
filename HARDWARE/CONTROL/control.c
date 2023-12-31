#include "parameters.h"

#define TIMETOJUDGE 150

u8 auto_flow_arry[13]={20,40,50,100,120,120,120,120,120,120,120,120,120};//减去的值是根据流量来递增的

extern u8 xyiconstatue;
extern u8 bjiconstatue;
extern u8 getpressureflag;

extern u16 pressuresetvalue;
extern u8 xiyinflag;
extern u16 motorpscstart;
u8 xiyinonceflag=RESET;
u16 overprussuretimes = 0;

 
float KalmanFilter(float inData) 
{
	static float prevData=0; 
	static float p=10, q=0.0001, r=0.005, kGain=0;
    p = p+q; 
    kGain = p/(p+r);

    inData = prevData+(kGain*(inData-prevData)); 
    p = (1-kGain)*p;

    prevData = inData;

    return inData; 
}

float value_buf[NUM];
u16 filter_i = 0;
float filter(void) 
{
    u16 count = 0;
    float sum = 0;

    value_buf[ filter_i++ ] = ChuliSensorPrussure();//get_ad();
    if( filter_i == NUM ) {
        filter_i = 0;    //先进先出
    }
    for( count = 0; count < NUM; count++ ) 
	{
        sum += value_buf[count];
    }
    return (float)(sum / NUM);
}

float Auto_pressure_adjust(float sensorpressureval,u8 presetflowval)
{
    float sensor_val;
    
    switch( presetflowval )
    {
        case 1:  sensor_val = sensorpressureval - auto_flow_arry[0];  break;
        case 2:  sensor_val = sensorpressureval - auto_flow_arry[1];  break;
        case 3:  sensor_val = sensorpressureval - auto_flow_arry[2];  break;
        case 4:  sensor_val = sensorpressureval - auto_flow_arry[3];  break;
        case 5:  sensor_val = sensorpressureval - auto_flow_arry[4];  break;
        case 6:  sensor_val = sensorpressureval - auto_flow_arry[5];  break;
        case 7:  sensor_val = sensorpressureval - auto_flow_arry[6];  break;
        case 8:  sensor_val = sensorpressureval - auto_flow_arry[7];  break;
        case 9:  sensor_val = sensorpressureval - auto_flow_arry[8];  break;
        case 10: sensor_val = sensorpressureval - auto_flow_arry[9];  break;
        case 11: sensor_val = sensorpressureval - auto_flow_arry[10]; break;
        case 12: sensor_val = sensorpressureval - auto_flow_arry[11]; break;
        case 13: sensor_val = sensorpressureval - auto_flow_arry[12]; break;
        default: break;
    }
    if( sensor_val < 0 ) return 0;//结果如果是负数，强制为0
    else return sensor_val;     
}


float chulival_gal;    
u8 motogear = 1;
u8 flowmode;
u8 display_delay_flag = 0;
u8 speedupflag = 0;

void MotorControl()
{
    static float motorreloadvalue;
	
    if( bjiconstatue&0x01 )//开始按键按下
    {
        if( pressuresetvalue > (u16)chuli_current_val )//Ps>Pr   实际压力 < 预设压力  1、起始阶段
        {			

/***********************************************当 实际压力 < 预设压力-10 ************************************************************************/			
            if( (u16)chuli_current_val < ( pressuresetvalue - 5 ))//实际压力比预设压力小10
            { 
                flowmode = 1;
				if(speedupflag == 1)
				{
					motorreloadvalue = Motor_PscCount( motogear );
					speedupflag = 0;
					MotorSpeedUp(motorreloadvalue);
					motorreloadvalue = motorpscstart;//
				}
                Ps_PrGreaterFive( motorreloadvalue );//Motor_PscCount()rely on flow set value then calculate motor speed      
            }

/*******************************当 预设压力-5 < 实际压力 < 预设压力 时***********************************************************/
			else if( ( pressuresetvalue - 5 ) <= (u16)chuli_current_val )//当 预设压力-5 < 实际压力 < 预设压力 时, 进入慢补
			{  
              
				ZeroLessPs_PrlessFive();//进入慢步
			} 
        }
/***********************************************当 实际压力 > 预设压力 时***********************************************************/
        else//Ps<Pr
        {   
            mypid.LastError = 0;
            mypid.PrevError = 0;            
            if(((u16)chuli_current_val - pressuresetvalue) <= 2 )
            {				
				ZerolessPr_PsLessTwo();//5)2≥P实际-P预设≥0，停转
            }
            else if( 2<((u16)chuli_current_val-pressuresetvalue) && ((u16)chuli_current_val-pressuresetvalue) < 5 )
            {
				TwolessPr_PsLessFive();//5＞P实际-P预设≥2 按0.1流量反转，此条件如保持3秒，则压力条黄色，低级报警声音（见图3）；
            }
            else
            {   
				ThirtylessPr_Ps();//8)P实际-P预设≥5 按0.1流量反转，此条件如保持3秒，则压力条红色，中级报警声音（见图3）�
//				while(((u16)chuli_current_val) > pressuresetvalue - 20)
//				{
//					delay_ms(1);
//				}
//				motogear--;//将流量设置暂存到一个变量中
//				if(motogear<=1)
//					motogear=1;
            }  
        }
    }	
/***********************************************当电机按钮关掉之后时***********************************************************/	
    else
    {      
		mypid.SumError = 0;  
        mypid.LastError = 0;
        mypid.PrevError = 0;         
		motogear = flowsetvalue;
		flowmode = 0;
		speedupflag = 1;
        motorpscstart = MOTORINITPSC;
        DisableMotorPWM();
    }
}

float KalmanFilter1(float inData) 
{
	static float prevData=0; 
	static float p=10, q=0.00000001, r=0.5, kGain=0;
    p = p+q; 
    kGain = p/(p+r);

    inData = prevData+(kGain*(inData-prevData)); 
    p = (1-kGain)*p;

    prevData = inData;

    return inData; 
}

float speedoffset=0;
//3)P预设-P实际＞5，按设定流量转动
void Ps_PrGreaterFive(u16 FLset3)
{

	static u16 speedjudgecount=0;
	
    overprussuretimes = 0;
    DisableBuzzer();//disable buzzer 
    CTRL_DIR=1;//逆时针
	EnableMotorPWM();
	
	PWM_out_Vla = PIDCalc( &mypid,chuli_current_val );
	
    PWM_out_Vla = KalmanFilter1(PWM_out_Vla);
    
	if(PWM_out_Vla > 720 )
	{
		PWM_out_Vla = 720;
	}
	else if(PWM_out_Vla < FLOW_13L_SPEED)
	{
		PWM_out_Vla = FLOW_13L_SPEED;
	}
	
	PWM_out_Vla = 720 - PWM_out_Vla;
	
	if(PWM_out_Vla < (FLset3) )//想快但是快不了，最快的速度只能是退出加速时的速度
	{
		PWM_out_Vla = FLset3;//限速
		
//		if((u16)chuli_current_val < ( pressuresetvalue - 20 ))
//		{
//			delay_ms(1);
//			speedjudgecount++;
//			if(speedjudgecount%200 == 0)
//				speedoffset--;
//			if(speedoffset < -150)
//				speedoffset = -150;
//		}
//		else if( ( pressuresetvalue - 10 ) < (u16)chuli_current_val &&  (u16)chuli_current_val < ( pressuresetvalue - 5 ))
//		{
//			delay_ms(1);
//			speedjudgecount++;
//			if(speedjudgecount%200 == 0)
//				speedoffset++;
//			if(speedoffset > 0)
//				speedoffset = 0;
//		}
	}
    else
    {
		if((u16)chuli_current_val < ( pressuresetvalue - 20 ))
		{
			delay_ms(1);
			speedjudgecount++;
			if(speedjudgecount%200 == 0)
				speedoffset--;
            PWM_out_Vla += speedoffset;
			if(speedoffset < -200)
				speedoffset = -200;
            if(PWM_out_Vla < (FLset3) )
                PWM_out_Vla = FLset3;//限速
        }
        
//        PWM_out_Vla = PWM_out_Vla - (PWM_out_Vla - FLset3 )/1.2;//如果计算出的速度比退出加速时的还要慢，就使用计算出的速度
    }
    
	motorpscstart = (u16)PWM_out_Vla;	
		
}
//4)0<P预设-P实际≤5，按0.1L/min转动（无论流量设定多大）
void ZeroLessPs_PrlessFive()
{
    speedoffset=0;
    overprussuretimes = 0;
    DisableBuzzer();//disable buzzer 
    flowmode = 0; 
    motorpscstart = MOTORINITPSC;
    CTRL_DIR=1;//逆时针
    EnableMotorPWM();
    MotorSpeedDownlmin();//0.1l/min
}
//5)2≥P实际-P预设≥0，停转
void ZerolessPr_PsLessTwo()
{    
    speedoffset=0;
    overprussuretimes = 0;
    flowmode = 0;
    DisableBuzzer();//disable buzzer 
    motorpscstart = MOTORINITPSC;
    DisableMotorPWM();
}
//6)5＞P实际-P预设＞2 按0.1流量反转，压力条绿色
void TwolessPr_PsLessFive()
{
    speedoffset=0;
    overprussuretimes = 0;
    flowmode = 0;
    DisableBuzzer();//disable buzzer 
    motorpscstart = MOTORINITPSC;
    CTRL_DIR=0;//顺时针
    EnableMotorPWM();
    Motor0_1lmin();//0.1l/min
}
//7)30＞P实际-P预设≥5 按0.1流量反转，此条件如保持3秒，则压力条黄色，低级报警声音（见图3）；
void FivelessPr_PsLessThirty()
{
    speedoffset=0;
    motorpscstart=MOTORINITPSC;
    flowmode = 0;
    CTRL_DIR=0;//顺时针
    EnableMotorPWM();
    Motor0_1lmin();//0.1l/min
    delay_ms(1);
    overprussuretimes++;
    if(overprussuretimes>=3000)
    {
        overprussuretimes=3000;
        Buzzer_Mode(BUZZER_MODE_LOW);//enable buzzer ,buzzer mode low
    }   
}
//8)P实际-P预设≥30 按0.1流量反转，此条件如保持3秒，则压力条红色，中级报警声音（见图3）；
void ThirtylessPr_Ps()
{
    speedoffset=0;
    motorpscstart=MOTORINITPSC;
    flowmode = 0;
    CTRL_DIR=0;//顺时针
    EnableMotorPWM();
    Motor0_1lmin();//0.1l/min
    delay_ms(1);
    overprussuretimes++;
    if(overprussuretimes >= 3000)
    {
        overprussuretimes = 3000;
        Buzzer_Mode(BUZZER_MODE_LOW);//enable buzzer ,buzzer mode low
    }
}

