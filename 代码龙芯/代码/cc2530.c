/*********************************************************************
 * INCLUDES
 */

#include <stdio.h>
#include <string.h>
#include "AF.h"
#include "OnBoard.h"
#include "OSAL_Tasks.h"
#include "SampleApp.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "hal_drivers.h"
#include "hal_key.h"
#if defined ( LCD_SUPPORTED )
  #include "hal_lcd.h"
#endif
#include "hal_led.h"
#include "hal_uart.h"
#include "dht11.h"


/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

#if !defined( SAMPLE_APP_PORT )
#define SAMPLE_APP_PORT  0
#endif

#if !defined( SAMPLE_APP_BAUD )
  #define SAMPLE_APP_BAUD  HAL_UART_BR_115200
#endif

// When the Rx buf space is less than this threshold, invoke the Rx callback.
#if !defined( SAMPLE_APP_THRESH )
#define SAMPLE_APP_THRESH  64
#endif

#if !defined( SAMPLE_APP_RX_SZ )
#define SAMPLE_APP_RX_SZ  128
#endif

#if !defined( SAMPLE_APP_TX_SZ )
#define SAMPLE_APP_TX_SZ  128
#endif

// Millisecs of idle time after a byte is received before invoking Rx callback.
#if !defined( SAMPLE_APP_IDLE )
#define SAMPLE_APP_IDLE  6
#endif

// Loopback Rx bytes to Tx for throughput testing.
#if !defined( SAMPLE_APP_LOOPBACK )
#define SAMPLE_APP_LOOPBACK  FALSE
#endif

// This is the max byte count per OTA message.
#if !defined( SAMPLE_APP_TX_MAX )
#define SAMPLE_APP_TX_MAX  80
#endif

#define SAMPLE_APP_RSP_CNT  4

// This list should be filled with Application specific Cluster IDs.
const cId_t SampleApp_ClusterList[SAMPLE_MAX_CLUSTERS] =
{
  SAMPLEAPP_P2P_CLUSTERID,
  SAMPLEAPP_PERIODIC_CLUSTERID,
};

const SimpleDescriptionFormat_t SampleApp_SimpleDesc =
{
  SAMPLEAPP_ENDPOINT,              //  int   Endpoint;
  SAMPLEAPP_PROFID,                //  uint16 AppProfId[2];
  SAMPLEAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  SAMPLEAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  SAMPLEAPP_FLAGS,                 //  int   AppFlags:4;
  SAMPLE_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)SampleApp_ClusterList,  //  byte *pAppInClusterList;
  SAMPLE_MAX_CLUSTERS,          //  byte  AppNumOutClusters;
  (cId_t *)SampleApp_ClusterList   //  byte *pAppOutClusterList;
};

endPointDesc_t SampleApp_epDesc =
{
  SAMPLEAPP_ENDPOINT,
 &SampleApp_TaskID,
  (SimpleDescriptionFormat_t *)&SampleApp_SimpleDesc,
  noLatencyReqs
};

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
devStates_t SampleApp_NwkState;   
uint8 SampleApp_TaskID;           // Task ID for internal task/event processing.

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static uint8 SampleApp_MsgID;

afAddrType_t SampleApp_Periodic_DstAddr; //广播
afAddrType_t SampleApp_Flash_DstAddr;    //组播
afAddrType_t SampleApp_P2P_DstAddr;      //点播


static afAddrType_t SampleApp_TxAddr;
static uint8 SampleApp_TxSeq;
static uint8 SampleApp_TxBuf[SAMPLE_APP_TX_MAX+1];
static uint8 SampleApp_TxLen;

static afAddrType_t SampleApp_RxAddr;
static uint8 SampleApp_RxSeq;
static uint8 SampleApp_RspBuf[SAMPLE_APP_RSP_CNT];

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void SampleApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt );
void SampleApp_CallBack(uint8 port, uint8 event); 
static void SampleApp_Send_P2P_Message( void );
static void packDataAndSend(uint8 fc, uint8* data, uint8 len);


/*********************************************************************
 * @fn      SampleApp_Init
 *
 * @brief   This is called during OSAL tasks' initialization.
 *
 * @param   task_id - the Task ID assigned by OSAL.
 *
 * @return  none
 */
void SampleApp_Init( uint8 task_id )
{
  halUARTCfg_t uartConfig;

  SampleApp_TaskID = task_id;
  SampleApp_RxSeq = 0xC3;
  SampleApp_NwkState = DEV_INIT;       

  MT_UartInit();                  //串口初始化
  MT_UartRegisterTaskID(task_id); //注册串口任务
  afRegister( (endPointDesc_t *)&SampleApp_epDesc );
  RegisterForKeys( task_id );

#ifdef ZDO_COORDINATOR
  //协调器初始化

  P0SEL &= ~0x80;                 //设置P07为普通IO口
  P0DIR |= 0x80;                 //P07定义为输出口  
#endif

  SampleApp_Periodic_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;//广播
  SampleApp_Periodic_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_Periodic_DstAddr.addr.shortAddr = 0xFFFF;

  // Setup for the flash command's destination address - Group 1
  SampleApp_Flash_DstAddr.addrMode = (afAddrMode_t)afAddrGroup;//组播
  SampleApp_Flash_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
  SampleApp_Flash_DstAddr.addr.shortAddr = SAMPLEAPP_FLASH_GROUP;
  
  SampleApp_P2P_DstAddr.addrMode = (afAddrMode_t)Addr16Bit; //点播 
  SampleApp_P2P_DstAddr.endPoint = SAMPLEAPP_ENDPOINT; 
  SampleApp_P2P_DstAddr.addr.shortAddr = 0x0000;            //发给协调器

  
}

/*********************************************************************
 * @fn      SampleApp_ProcessEvent
 *
 * @brief   Generic Application Task event processor.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events   - Bit map of events to process.
 *
 * @return  Event flags of all unprocessed events.
 */
UINT16 SampleApp_ProcessEvent( uint8 task_id, UINT16 events )
{
  (void)task_id;  // Intentionally unreferenced parameter
  
  if ( events & SYS_EVENT_MSG )
  {
    afIncomingMSGPacket_t *MSGpkt;

    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( SampleApp_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
      case AF_INCOMING_MSG_CMD:
        SampleApp_ProcessMSGCmd( MSGpkt );
        break;
        
      case ZDO_STATE_CHANGE:
        SampleApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
        if ( //(SampleApp_NwkState == DEV_ZB_COORD)||
            (SampleApp_NwkState == DEV_ROUTER)
            || (SampleApp_NwkState == DEV_END_DEVICE) )
        {
            //连网成功后，启动一个定时器
            osal_start_timerEx( SampleApp_TaskID,
                              SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
                              SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT );
        }
        else
        {
          // Device is no longer in the network
        }
        break;

      default:
        break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt );
    }

    return ( events ^ SYS_EVENT_MSG );
  }

  //定时器时间到
  if ( events & SAMPLEAPP_SEND_PERIODIC_MSG_EVT )
  {
    // DHT11采集
    SampleApp_Send_P2P_Message();

    // Setup to send message again in normal period (+ a little jitter)
    osal_start_timerEx( SampleApp_TaskID, SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
        (SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT + (osal_rand() & 0x00FF)) );

    // return unprocessed events
    return (events ^ SAMPLEAPP_SEND_PERIODIC_MSG_EVT);
  }


  return ( 0 );  // Discard unknown events.
}

/*********************************************************************
 * @fn      SerialApp_ProcessMSGCmd
 *
 * @brief   Data message processor callback. This function processes
 *          any incoming data - probably from other devices. Based
 *          on the cluster ID, perform the intended action.
 *
 * @param   pkt - pointer to the incoming message packet
 *
 * @return  TRUE if the 'pkt' parameter is being used and will be freed later,
 *          FALSE otherwise.
 */
void SampleApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt )
{
  uint8 buff[20]={0};

  switch ( pkt->clusterId )
  {
  // 接收终端上传的温度数据
  case SAMPLEAPP_P2P_CLUSTERID: 
#ifdef ZDO_COORDINATOR
    {
        uint8 id=pkt->cmd.Data[0];//终端id
        uint8 t=pkt->cmd.Data[1]; //终端温度
        uint8 h=pkt->cmd.Data[2]; //终端湿度 

        if(id==1)
        {
          sprintf(buff, "%d T&H:%d %d", id, t, h);
          HalLcdWriteString(buff, HAL_LCD_LINE_2); //LCD显示
        }
        else if(id==2)
        {
          sprintf(buff, "%d T&H:%d %d", id, t, h);
          HalLcdWriteString(buff, HAL_LCD_LINE_3); //LCD显示
        }
        else if(id==3)
        {
          sprintf(buff, "%d T&H:%d %d", id, t, h);
          HalLcdWriteString(buff, HAL_LCD_LINE_4); //LCD显示
        }

        
        HalUARTWrite(0, buff, osal_strlen(buff));           //串口输出提示信息
        HalUARTWrite(0, "\r\n",2);
#else        
        //打包无线发送的数据        
        buff[0]=id;//终端id
        buff[1]=t; //终端温度
        buff[2]=h; //终端湿度

        //打包数据用于发送到wifi网关，或者串口
        packDataAndSend(FUN_CODE_UPDATA_DATA, buff, 3);
      
#endif
    }
#endif
    break;

  case SAMPLEAPP_PERIODIC_CLUSTERID:

    break;

    default:
      break;
  }
}


/*********************************************************************
 * @fn      SampleApp_CallBack
 *
 * @brief   Send data OTA.
 *
 * @param   port - UART port.
 * @param   event - the UART port event flag.
 *
 * @return  none
 */
void SampleApp_CallBack(uint8 port, uint8 event)
{
  (void)port;

  if ((event & (HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT)) &&
#if SAMPLE_APP_LOOPBACK
      (SampleApp_TxLen < SAMPLE_APP_TX_MAX))
#else
      !SampleApp_TxLen)
#endif
  {
    SampleApp_TxLen += HalUARTRead(SAMPLE_APP_PORT, SampleApp_TxBuf+SampleApp_TxLen+1,
                                                      SAMPLE_APP_TX_MAX-SampleApp_TxLen);
  }
}

/*********************************************************************
 * @fn      SampleApp_Send_P2P_Message
 *
 * @brief   point to point.
 *
 * @param   none
 *
 * @return  none
 */
void SampleApp_Send_P2P_Message( void )
{
  uint8 str[5]={0};
  uint8 strTemp[20]={0};
  int len=0;

  DHT11();             //获取温湿度

  str[0] = 1;//增加一个ID，如果是多个终端就增加这个值
  str[1] = wendu;//温度
  str[2] = shidu;//湿度
  len=3;

  sprintf(strTemp, "T&H:%d %d", str[1],str[2]);
  HalLcdWriteString(strTemp, HAL_LCD_LINE_3); //LCD显示

  HalUARTWrite(0, strTemp, osal_strlen(strTemp));           //串口输出提示信息
  HalUARTWrite(0, "\r\n",2);

  //无线发送到协调器
  if ( AF_DataRequest( &SampleApp_P2P_DstAddr, &SampleApp_epDesc,
                       SAMPLEAPP_P2P_CLUSTERID,
                       len,
                       str,
                       &SampleApp_MsgID,
                       AF_DISCV_ROUTE,
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
}

uint8 CheckSum(uint8 *pdata, uint8 len)
{
	uint8 i;
	uint8 check_sum=0;

	for(i=0; i<len; i++)
	{
		check_sum += pdata[i];
	}
	return check_sum;
}

//数据打包发送
/**
*fc:功能码
*data:上传的数据
*len:数据长度
格式:len,校验,fc,内容,$,@,
*/
void packDataAndSend(uint8 fc, uint8* data, uint8 len)
{
    osal_memset(SampleApp_TxBuf, 0, SAMPLE_APP_TX_MAX+1);


    //数据包长度
    SampleApp_TxBuf[0]=3+len;

    //功能码
    SampleApp_TxBuf[2]=fc;

    //发送的数据
    if(len>0)
    {
        osal_memcpy(SampleApp_TxBuf+3, data, len);
    }

    //校验和,从fc开始，
    SampleApp_TxBuf[1]=CheckSum(SampleApp_TxBuf+2, len+1);

    //数据结尾
    SampleApp_TxBuf[3+len]='$';
    SampleApp_TxBuf[4+len]='@';

    //发送长度
    SampleApp_TxLen=5+len;

    //接着发数据包
    HalUARTWrite(0,SampleApp_TxBuf, SampleApp_TxLen);
}

