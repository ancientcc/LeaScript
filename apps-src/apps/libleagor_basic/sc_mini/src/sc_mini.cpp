#include <ros/ros.h>
#include <ros/callback_queue.h>
#include <math.h>
#include <string.h>
#include <std_msgs/UInt32.h>
#include <std_msgs/Float32.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Vector3.h>
#include <geometry_msgs/Quaternion.h>
#include <tf/transform_datatypes.h>
#include <sensor_msgs/LaserScan.h>
#include "../include/sc_mini/sc_mini.h"
#include <std_msgs/Int32.h>
#include <std_msgs/Int32MultiArray.h>
#include <iostream>
/*
#include <stdio.h>                                                                                                
#include <stdlib.h>     
#include <unistd.h>     
#include <sys/types.h>  
#include <sys/stat.h>   
#include <fcntl.h>      
#include <termios.h>    
#include <errno.h>
#include <vector>
#include <arpa/inet.h>
#include <sstream>
*/

//#define ANGLE_MAX_NUMBERS 4000                                                               
#define ROS_N_MUL 2
#define POINTS_OF_HEAD 3
#define B_HEAD0 0x55
#define B_HEAD1 0xAA
#define B_TAIL 0xFA
#define AngleZoneNumber 15
#define AngleSizePerZone (360.0F/AngleZoneNumber)
#define AngleZoneSampleNo 30
#define COM_FRAME_LEN (2*AngleZoneSampleNo + 9)

using namespace std;

EaiDataStr EaiData = {0};


const int robot_rotate_compensate_dir = +1;


const float Notch_angle[6][2]={
	17.11f, 32.59f,
	98.25f, 116.95f,
	147.65f, 163.02f,
	230.08f, 247.76f,
	280.60f, 297.60f,
	326.50f, 346.38f
};

const int enable_cover = 0;


float degree_cali= 0.0;

//////////////////////////////////////////////

uint16_t start=0xEE;
uint16_t stop=0xEF;
float PI = 3.1415926f;
double angle_yaw_new = 0; //note: 
double angle_yaw_old = 0; //note: 
double zone_start_rotate_angle = 0; //note: 
int Size;
int lidar_start_count;
double clustering_time=0;


ros::Time lidar_cali_time;

ros::Publisher lidarZero_pub;
ros::Publisher robotErrorTips_pub;


int AllAngleIndex = 0;

SDL_handle open_port(const char *port, int nSpeed)
{
	SDL_handle fd;
	fd=SDL_OpenSerialPort(port, nSpeed);
	if(fd==SDL_INVALID_HANDLE_VALUE)
	{
		perror("Can't Open SerialPort");
	}

	return fd;
}
 
void odomCallback(const nav_msgs::Odometry::ConstPtr& odom)
{
	geometry_msgs::Quaternion quat;
	quat.x=odom->pose.pose.orientation.x;
	quat.y=odom->pose.pose.orientation.y;
	quat.z=odom->pose.pose.orientation.z; 
	quat.w=odom->pose.pose.orientation.w;
	tf::Quaternion quat_tf;
	tf::quaternionMsgToTF(quat,quat_tf);
	double roll,pitch,yaw = 0;
	tf::Matrix3x3(quat_tf).getRPY(roll,pitch,yaw);
	angle_yaw_new = (180/PI)*yaw;
}

namespace sc_m_c
{
	SCLaser::SCLaser()
	{

	}

	SCLaser::~SCLaser()
	{
		
	}
	void PutRemainder2Start(unsigned char *btBuffer,int nStart,int nBufLength)
	{
		if (nStart>0)
		{
			for (int i=nStart;i<nStart+nBufLength;i++)
			{   
				btBuffer[i-nStart] = btBuffer[i];
			}   
		}   

	} 

	uint8_t MatchZoneNo = 0;
	int m_nReceivedLength=0; //接收雷达点云数据的接收缓冲区里缓冲的数据长度
	int nSeekStart = 0;
	char raw_bytes[ANGLE_MAX_NUMBERS]; //接收雷达点云数据的接收缓冲区
	float Angle_in[ANGLE_MAX_NUMBERS];
	float FilterRatio = 2.5; //滤波功能用到的滤波系数设置为2.5
	
	void SCLaser::PointCloudFilter(sensor_msgs::LaserScan::Ptr Scan)
	{
		int i0, i1, i2, i3, i4;
		float x0,y0, x1,y1, x2,y2, x3,y3, x4,y4, r1, r2;
		float d01, d12, d23, d34;
		float a, b, Adeta;
		// char StrBuf[100];
		float FilterRatioAdj;
		float AngDiff;
		int DepthState;

		AngDiff  = 2*PI/(Size - 1);
		FilterRatioAdj = FilterRatio * sin(AngDiff);

		for(i0=0; i0<Size; i0++)
		{
			i1 = i0+1; i1=i1<Size ? i1 : i1-Size;
			i2 = i0+2; i2=i2<Size ? i2 : i2-Size;
			i3 = i0+3; i3=i3<Size ? i3 : i3-Size;
			DepthState = 0;
			DepthState += (Scan->ranges[i0] != 0)*1;
			DepthState += (Scan->ranges[i1] != 0)*2;
			DepthState += (Scan->ranges[i2] != 0)*4;
			DepthState += (Scan->ranges[i3] != 0)*8;
			if(DepthState == 0x0F)
			{//1111
				x0 = Scan->ranges[i0] * cos(Angle_in[i0]*PI/180);
				y0 = Scan->ranges[i0] * sin(Angle_in[i0]*PI/180);
				x3 = Scan->ranges[i3] * cos(Angle_in[i3]*PI/180);
				y3 = Scan->ranges[i3] * sin(Angle_in[i3]*PI/180);
				x1 = (x3+2*x0)/3;
				y1 = (y3+2*y0)/3;
				x2 = (2*x3+x0)/3;
				y2 = (2*y3+y0)/3;
				r1 = sqrt(x1*x1 + y1*y1);
				r2 = sqrt(x2*x2 + y2*y2);

				a = Scan->ranges[i0];
				b = Scan->ranges[i1];
				Adeta = abs(Angle_in[i0] - Angle_in[i1]) * PI/180;
				d01 = sqrt(a*a + b*b -2*a*b*cos(Adeta));
				a = Scan->ranges[i1];
				b = Scan->ranges[i2];
				Adeta = abs(Angle_in[i1] - Angle_in[i2]) * PI/180;
				d12 = sqrt(a*a + b*b -2*a*b*cos(Adeta));
				a = Scan->ranges[i2];
				b = Scan->ranges[i3];
				Adeta = abs(Angle_in[i2] - Angle_in[i3]) * PI/180;
				d23 = sqrt(a*a + b*b -2*a*b*cos(Adeta));

				if(FilterRatioAdj * Scan->ranges[i1] < d01 && FilterRatioAdj * Scan->ranges[i1] < d12)
				{
					Scan->ranges[i1] = 0;
				}
				if(FilterRatioAdj * Scan->ranges[i2] < d12 && FilterRatioAdj * Scan->ranges[i2] < d23)
				{
					Scan->ranges[i2] = 0;
				}
				if(0 == Scan->ranges[i1] || 0 == Scan->ranges[i2])
				{
					continue;
				}

				if( (Scan->ranges[i1] > r1 && Scan->ranges[i2] < r2) || (Scan->ranges[i1] < r1 && Scan->ranges[i2] > r2) )
				{
					if(
							(d01 > FilterRatioAdj*Scan->ranges[i0] && d12 < FilterRatioAdj*Scan->ranges[i1] && d23 < FilterRatioAdj*Scan->ranges[i2])
							|| (d01 < FilterRatioAdj*Scan->ranges[i0] && d12 > FilterRatioAdj*Scan->ranges[i1] && d23 < FilterRatioAdj*Scan->ranges[i2])
							|| (d01 < FilterRatioAdj*Scan->ranges[i0] && d12 < FilterRatioAdj*Scan->ranges[i1] && d23 > FilterRatioAdj*Scan->ranges[i2])
					  )
					{
					}
					else
					{
						Scan->ranges[i1] = r1 + 0.4*(Scan->ranges[i1] - r1);
						Scan->ranges[i2] = r2 + 0.4*(Scan->ranges[i2] - r2);
					}
				}
				else
				{
				}
			}
			else if(DepthState == 0x00 || DepthState == 0x08 || DepthState == 0x01) 
			{// 0000 1000 0001
			}
			else if((DepthState & 0x0E) == 0x04)
			{//010x
				Scan->ranges[i2] = 0;
			}
			else if((DepthState & 0x07) == 0x02)
			{//x010
				Scan->ranges[i1] = 0;
			}
			else if((DepthState & 0x07) == 0x03)
			{//x011
				a = Scan->ranges[i0];
				b = Scan->ranges[i1];
				if(sqrt(a*a + b*b - 2*a*b*cos(AngDiff)) > FilterRatioAdj * Scan->ranges[i1])
				{
					Scan->ranges[i1] = 0;
				}
			}
			else if(DepthState == 0x0E)
			{//1110
				a = Scan->ranges[i1];
				b = Scan->ranges[i2];
				if(sqrt(a*a + b*b - 2*a*b*cos(AngDiff)) > FilterRatioAdj * Scan->ranges[i1])
				{
					Scan->ranges[i1] = 0;
				}
			}
			else if((DepthState & 0x0E) == 0x0C)
			{//110x 0111
				a = Scan->ranges[i2];
				b = Scan->ranges[i3];
				if(sqrt(a*a + b*b - 2*a*b*cos(AngDiff)) > FilterRatioAdj * Scan->ranges[i2])
				{
					Scan->ranges[i2] = 0;
				}
			}
			else if(DepthState == 0x07)
			{//0111
				a = Scan->ranges[i1];
				b = Scan->ranges[i2];
				if(sqrt(a*a + b*b - 2*a*b*cos(AngDiff)) > FilterRatioAdj * Scan->ranges[i2])
				{
					Scan->ranges[i2] = 0;
				}
			}
			else if(DepthState == 0x06)
			{//0110
				a = Scan->ranges[i1];
				b = Scan->ranges[i2];
				if(sqrt(a*a + b*b - 2*a*b*cos(AngDiff)) > FilterRatioAdj * Scan->ranges[i1])
				{
					Scan->ranges[i1] = 0;
					Scan->ranges[i2] = 0;
				}
			}
			else
			{
			}
		}

		//五点拉直

		for(i0=0; i0<Size; i0++)
		{
			i1 = i0+1; i1=i1<Size ? i1 : i1-Size;
			i2 = i0+2; i2=i2<Size ? i2 : i2-Size;
			i3 = i0+3; i3=i3<Size ? i3 : i3-Size;
			i4 = i0+4; i4=i4<Size ? i4 : i4-Size;
			if(Scan->ranges[i0] !=0 && Scan->ranges[i1] !=0 && Scan->ranges[i2] !=0 && Scan->ranges[i3] !=0 && Scan->ranges[i4] !=0)
			{
				a = Scan->ranges[i0];
				b = Scan->ranges[i1];
				Adeta = abs(Angle_in[i0] - Angle_in[i1]) * PI/180;
				d01 = sqrt(a*a + b*b -2*a*b*cos(Adeta));

				a = Scan->ranges[i1];
				b = Scan->ranges[i2];
				Adeta = abs(Angle_in[i1] - Angle_in[i2]) * PI/180;
				d12 = sqrt(a*a + b*b -2*a*b*cos(Adeta));
				a = Scan->ranges[i2];
				b = Scan->ranges[i3];
				Adeta = abs(Angle_in[i2] - Angle_in[i3]) * PI/180;
				d23 = sqrt(a*a + b*b -2*a*b*cos(Adeta));
				a = Scan->ranges[i3];
				b = Scan->ranges[i4];
				Adeta = abs(Angle_in[i3] - Angle_in[i4]) * PI/180;
				d34 = sqrt(a*a + b*b -2*a*b*cos(Adeta));
				if(
						d01 < FilterRatioAdj * Scan->ranges[i1] && d34 < FilterRatioAdj * Scan->ranges[i3]
						&& (d12 > FilterRatioAdj * Scan->ranges[i2] || d23 > FilterRatioAdj * Scan->ranges[i2])
				  )
				{
					if(
							(Scan->ranges[i0] < Scan->ranges[i1] && Scan->ranges[i3] < Scan->ranges[i4])
							|| (Scan->ranges[i0] > Scan->ranges[i1] && Scan->ranges[i3] > Scan->ranges[i4])
					  )
					{
						Scan->ranges[i2] = (Scan->ranges[i1] + Scan->ranges[i3])/2;
					}
				}
			}
			else if(Scan->ranges[i0] ==0 && Scan->ranges[i1] !=0 && Scan->ranges[i2] !=0 && Scan->ranges[i3] !=0 && Scan->ranges[i4] !=0)
			{
				x2 = Scan->ranges[i2] * cos(Angle_in[i2]*PI/180);
				y2 = Scan->ranges[i2] * sin(Angle_in[i2]*PI/180);
				x3 = Scan->ranges[i3] * cos(Angle_in[i3]*PI/180);
				y3 = Scan->ranges[i3] * sin(Angle_in[i3]*PI/180);
				x1 = 2*x2 - x3;
				y1 = 2*y2 - y3;
				Scan->ranges[i1] = Scan->ranges[i1] + 0.4 * (sqrt(x1*x1 + y1*y1) - Scan->ranges[i1]);
			}
			else if(Scan->ranges[i0] !=0 && Scan->ranges[i1] !=0 && Scan->ranges[i2] !=0 && Scan->ranges[i3] !=0 && Scan->ranges[i4] ==0)
			{
				x1 = Scan->ranges[i1] * cos(Angle_in[i1]*PI/180);
				y1 = Scan->ranges[i1] * sin(Angle_in[i1]*PI/180);
				x2 = Scan->ranges[i2] * cos(Angle_in[i2]*PI/180);
				y2 = Scan->ranges[i2] * sin(Angle_in[i2]*PI/180);
				x3 = 2*x2 - x1;
				y3 = 2*y2 - y1;
				Scan->ranges[i3] = Scan->ranges[i3] + 0.4 * (sqrt(x3*x3 + y3*y3) - Scan->ranges[i3]);
			}
			else
			{
			}
		}
	}
	/*角度插值函数
	* input: scan_in 指针
	* output: scan_out 指针
	* 功能: 将scan_in的点数扩大到400*ROS_N_MUL倍，然后角度插值，得到用于对外发布的scan_out
	*/
	void SCLaser::angle_insert(sensor_msgs::LaserScan::Ptr scan_in, sensor_msgs::LaserScan::Ptr scan_out)
	{
		int temp_i, i,angle,temp_Size;
		float m_fAngle;
		temp_Size = 400;  //note: 雷达转一圈输出的点数按固定的400*ROS_N_MUL点来输出

		scan_out->ranges.resize(temp_Size*ROS_N_MUL);
		scan_out->intensities.resize(temp_Size*ROS_N_MUL);

		for(i=0; i<temp_Size*ROS_N_MUL; i++)
		{
			scan_out->ranges[i] = 0; //note:将要发布的激光类距离数据清零，保证做插值时其他数据为零
		}
		for (i=0;i<Size;i++)
		{
			temp_i = (int)(Angle_in[i] / (360.0F/(ROS_N_MUL*temp_Size)) + 0.5); //note:计算出对应的插值的位置
			temp_i = (temp_i >= temp_Size*ROS_N_MUL) ? 0 : temp_i;
			temp_i = (temp_i < 0) ? 0 : temp_i;
			scan_out->ranges[temp_i] = scan_in->ranges[i];
			scan_out->intensities[temp_i] = scan_out->ranges[temp_i] == 0 ? 0 : 127; //note:距离值为0的数据灰度值为0，距离大于0数据灰度值设为127
		}

		scan_out->angle_increment = (2.0*M_PI/(ROS_N_MUL*temp_Size));
		scan_out->angle_min = 0.0f;
		scan_out->angle_max = 2*M_PI-scan_out->angle_increment;
		scan_out->range_min = 0.10;
		scan_out->range_max = 12.0; //note: mini_m1c0测量的最远距离是12m
		//scan_out->scan_time = clustering_time;
	}
	

	#define RANGES_TIMP_SIZE 500
	void SCLaser::poll(sensor_msgs::LaserScan::Ptr scan,SDL_handle fd, bool& exit)
	{
		int Rcv=0;
		int index, radius;
		bool FrameOK = false;
		char StrBuf[100];
		int CheckFlag=0;//
		bool WaitOneFrame = true;

		char ZoneNo;
		char SampleNumber;
		float Angle_fsa, Angle_lsa;
		short StartAngle;
		short StopAngle;
		float fStartAngle;
		float fIncAngle;
		float fStopAngle;
		float fTempAngle;
		float angle_check;
		float angle_c0,angle_c1;
		scan->ranges.resize(RANGES_TIMP_SIZE);
		while(!exit)
		{
			if(/*WaitOneFrame ||*/ EaiData.BufferLen < EAI_FRAME_LEN_MAX) 
			{
				int Rcv = SDL_ReadSerialPort(fd, raw_bytes, EAI_FRAME_LEN_MAX - EaiData.BufferLen);
				if(Rcv > 0)
				{

					for(int i=0;i<Rcv;i++)
					{
						EaiData.Buffer[EaiData.BufferLen+i]=(unsigned char)raw_bytes[i];
					}
					EaiData.BufferLen += Rcv;
				}
				WaitOneFrame = false;

			}
			else
			{
				int nSeekStart;
				for(nSeekStart = 0; nSeekStart < EaiData.BufferLen/* - EAI_FRAME_LEN_MIN*/; nSeekStart++)
				{
					if(EaiData.Buffer[nSeekStart] == 0xAA && EaiData.Buffer[nSeekStart+1] == 0x55)
					{
						CheckFlag = 0;
						if(nSeekStart != 0)
						{
							PutRemainder2Start(EaiData.Buffer, nSeekStart, EaiData.BufferLen - nSeekStart);
							EaiData.BufferLen = EaiData.BufferLen - nSeekStart;
							nSeekStart=0;
							if (EaiData.BufferLen < EAI_FRAME_LEN_MIN) {
								WaitOneFrame = true;
								break;
							}
						}
						if(EaiData.Frame.CT == 0x01 && EaiData.Frame.LSN == 0x01 
								&& (EaiData.Frame.FSAL & 0x01)
								&& (EaiData.Frame.LSAL & 0x01)
								&& EaiData.Frame.FSAL == EaiData.Frame.LSAL
								&& EaiData.Frame.FSAH == EaiData.Frame.LSAH
						  )
						{
							if(EaiData.Frame.CSL == EaiData.Frame.PHL^EaiData.Frame.CT^EaiData.Frame.FSAL^EaiData.Frame.LSAL^EaiData.Frame.Si[0]
									&& EaiData.Frame.CSH == EaiData.Frame.PHH^EaiData.Frame.LSN^EaiData.Frame.FSAH^EaiData.Frame.LSAH^EaiData.Frame.Si[1])
							{
								WaitOneFrame = true;
								radius = (EaiData.Frame.Si[0] | (EaiData.Frame.Si[1]<<8))/4;
								scan->ranges[AllAngleIndex] = radius;
								Angle_in[AllAngleIndex] = (EaiData.Frame.FSAL + EaiData.Frame.FSAH*256)/128.0;
								AllAngleIndex++;
								Size = AllAngleIndex > 0 ? AllAngleIndex : 1;
								EaiData.BufferLen = EaiData.BufferLen - EAI_FRAME_LEN_MIN;
								PutRemainder2Start(EaiData.Buffer, EAI_FRAME_LEN_MIN, EaiData.BufferLen);
								AllAngleIndex = 0;
								scan->angle_increment = (2.0*M_PI/Size);
								scan->angle_min = 0.0;
								scan->angle_max = 2*M_PI;
								scan->range_min = 0.10;
								scan->range_max = 12.0; //note: mini_m1c0
								return; //ok, receive full frame, exit to draw.
							}
							/*******************check failed******************/
							WaitOneFrame = true;
							EaiData.BufferLen = EaiData.BufferLen - EAI_FRAME_LEN_MIN;
							PutRemainder2Start(EaiData.Buffer, EAI_FRAME_LEN_MIN, EaiData.BufferLen);
							AllAngleIndex = 0;
							break;
							/*************************************************/
						}
						else if(EaiData.Frame.CT == 0x00 
								&& 0 < EaiData.Frame.LSN && EaiData.Frame.LSN < EAI_Si_iMAX
								&& (EaiData.Frame.FSAL & 0x01)
								&& (EaiData.Frame.LSAL & 0x01))
						{
							//ROS_INFO("5555555555555555555555555555555");
							if(2*EaiData.Frame.LSN + EAI_HEAD_LEN > EaiData.BufferLen - nSeekStart)
							{
								WaitOneFrame = true;
								EaiData.BufferLen = EaiData.BufferLen - nSeekStart;
								break;
							}
							CheckFlag = 1;
							Angle_fsa = (EaiData.Frame.FSAL + EaiData.Frame.FSAH*256)/128.0;
							Angle_lsa = (EaiData.Frame.LSAL + EaiData.Frame.LSAH*256)/128.0;
							//if(Angle_lsa > Angle_fsa)
							{
								unsigned char tempCSL, tempCSH;
								int j;
								tempCSL = EaiData.Frame.PHL^EaiData.Frame.CT^EaiData.Frame.FSAL^EaiData.Frame.LSAL;
								tempCSH = EaiData.Frame.PHH^EaiData.Frame.LSN^EaiData.Frame.FSAH^EaiData.Frame.LSAH;
								for(j=0; j<EaiData.Frame.LSN; j++)
								{
									tempCSL ^= EaiData.Frame.Si[2*j];
									tempCSH ^= EaiData.Frame.Si[2*j+1];
								}
								if(EaiData.Frame.CSL == tempCSL && EaiData.Frame.CSH == tempCSH)
								{ //ok, receive one frame
									CheckFlag = 2;
									fStartAngle = Angle_fsa;
									fStopAngle = Angle_lsa;
									
									if(EaiData.Frame.LSN == 1)
									{
										fIncAngle = 0;
									}
									else
									{
										if(fStartAngle>fStopAngle)
										{
											fIncAngle = (360 - fStartAngle + fStopAngle)/(EaiData.Frame.LSN-1);

										}
										else
										{
											fIncAngle = (fStopAngle - fStartAngle)/(EaiData.Frame.LSN-1);

										}
									}
									//fIncAngle = (fStopAngle - fStartAngle)/(EaiData.Frame.LSN-1);
									for(j=0; j<EaiData.Frame.LSN; j++)
									{
										//radius = (EaiData.Frame.Si[j] + 256*EaiData.Frame.Si[j+1])/16;
										radius = (EaiData.Frame.Si[2*j] | (EaiData.Frame.Si[2*j+1]<<8))/4;
										scan->ranges[AllAngleIndex] = radius/1000.0F;

										if(radius < 10)
										{
											angle_c0 = 0;
										}
										else
										{
											//mini_m1c0
											angle_c0 = (AllAngleIndex == 0) ? 0 : atan( 19.16 * ((float)radius - 90.15) / (90.15*(float)radius) ) * 180 / PI - 12;
									
											//m1c0
											//angle_c0 = (AllAngleIndex == 0) ? 0 : atan( 22.51 * ((float)radius - 170.98) / (170.98*(float)radius) ) * 180 / PI - 7.5;
										}

										fTempAngle = fStartAngle + j * fIncAngle;
										angle_check = 360 - fTempAngle; //note: 
										angle_c1 = angle_check + angle_c0 ; //note: 
										angle_c1 = angle_c1 > 360 ? (angle_c1-360) : angle_c1;
										angle_c1 = angle_c1 < 0 ? (angle_c1+360) : angle_c1;
										//fTempAngle = fTempAngle > 360 ? fTempAngle - 360 : fTempAngle;
										//printf("angle=%f fIncAngle=%f\n",fTempAngle,fIncAngle);
										if(enable_cover)
										{
											if(
												(Notch_angle[0][0] < angle_c1 && angle_c1 < Notch_angle[0][1])
												|| (Notch_angle[1][0] < angle_c1 && angle_c1 < Notch_angle[1][1])
												|| (Notch_angle[2][0] < angle_c1 && angle_c1 < Notch_angle[2][1])
												|| (Notch_angle[3][0] < angle_c1 && angle_c1 < Notch_angle[3][1])
												|| (Notch_angle[4][0] < angle_c1 && angle_c1 < Notch_angle[4][1])
												|| (Notch_angle[5][0] < angle_c1 && angle_c1 < Notch_angle[5][1])
											)
											{
												if(scan->ranges[AllAngleIndex] > 0.1)
												{
													scan->ranges[AllAngleIndex]=0;
												}
											}
										}
										angle_c1 += degree_cali;

										if(angle_c1 >= 360)
										{
											int tempint = angle_c1 / 360;
											angle_c1 = angle_c1 - tempint * 360;
										}
										if(angle_c1 < 0)
										{
											int tempint = angle_c1 / 360;
											angle_c1 = angle_c1 - (tempint - 1) * 360;
										}
										
										Angle_in[AllAngleIndex] = /*fTempAngle;*/ 360 - fTempAngle;
										AllAngleIndex++;
                                                                                

									}
									//WaitOneFrame = false;
								}
							}
							if(CheckFlag == 1)
							{

							}

						}
						else 
						{
						}
					}
					if(EaiData.BufferLen == (2*EaiData.Frame.LSN + EAI_HEAD_LEN))
					{
						WaitOneFrame = true;
						EaiData.BufferLen = 0;
						break;
					}
				}		
				if(WaitOneFrame == false)
				{
					EaiData.BufferLen = EaiData.BufferLen - nSeekStart;
					AllAngleIndex = 0;
				}
			}
		}
	}
}

ROSTIME_DECL int sc_mini__sc_mini(bool& exit, const std::string& serial_path, int baudrate)
{
	int argc = 0;
	ros::init(argc, nullptr, "sc_mini");

	SDL_handle fd;
	int baud_rate;
	std::string frame_id;
	std::string port;
	
	ros::NodeHandle n;
	ros::CallbackQueue cbqueue;
	n.setCallbackQueue(&cbqueue);

	ros::NodeHandle priv_nh("~");
	
	sensor_msgs::LaserScan::Ptr scan(new sensor_msgs::LaserScan);
	sensor_msgs::LaserScan::Ptr scan_publish(new sensor_msgs::LaserScan);
	ros::Publisher laser_pub = n.advertise<sensor_msgs::LaserScan>("scan", 1000);
	//
	ros::Subscriber sub = n.subscribe("odom", 1000, odomCallback);

	{
        // <clbrobot>/launch/lidar/rplidar.launch
        priv_nh.setParam("port", serial_path.empty()? "COM4": serial_path);
        priv_nh.setParam("baud_rate", baudrate);
        priv_nh.setParam("frame_id", "laser");
        priv_nh.setParam("inverted", false);
        priv_nh.setParam("angle_compensate", true);
        int ii = 0;
    }
	
	priv_nh.param("baud_rate", baud_rate, baudrate);
	priv_nh.param("frame_id", frame_id, std::string("laser_link"));
	priv_nh.param("port", port, std::string("/dev/sc_mini"));
	
	const char *port_t = port.c_str();

    	fd = open_port(port_t, baud_rate);
		if (fd == SDL_INVALID_HANDLE_VALUE) {
			ROS_INFO("sc_mini__sc_mini, open %s fail", port_t);
			return -1;
		}
    	// set_opt(fd,baud_rate,8,'N',1);
	
	sc_m_c::SCLaser laser;

	while (!exit && ros::ok())
	{   
		laser.poll(scan,fd, exit);
		laser.PointCloudFilter(scan);  //note:
		laser.angle_insert(scan,scan_publish); //note:
		scan_publish->header.frame_id = frame_id;
		scan_publish->header.stamp = ros::Time::now();
		laser_pub.publish(scan_publish);

		cbqueue.callAvailable(ros::WallDuration());
	}
        SDL_CloseSerialPort(fd);
	return 0;
}
