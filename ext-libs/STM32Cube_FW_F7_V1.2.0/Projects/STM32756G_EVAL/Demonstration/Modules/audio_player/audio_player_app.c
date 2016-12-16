/**
  ******************************************************************************
  * @file    audioplayer_app.c
  * @author  MCD Application Team
  * @version V1.2.0
  * @date    21-September-2015   
  * @brief   Audio player application functions
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */
  
/* Includes ------------------------------------------------------------------*/
#include "audio_player_app.h"
#include "stm32756g_eval_audio.h"
#include "k_bsp.h"
#include "k_rtc.h"

/** @addtogroup AUDIO_PLAYER_MODULE
  * @{
  */

/** @defgroup AUDIO_APPLICATION
  * @brief audio application routines
  * @{
  */


/* External variables --------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static FIL wav_file;
static AUDIOPLAYER_ProcessTypdef haudio;
static osMessageQId   AudioEvent;
static osThreadId     AudioThreadId = 0;

/* Private function prototypes -----------------------------------------------*/
static void Audio_Thread(void const * argument);

#if (!defined ( __GNUC__ ))
__IO uint32_t  AUDIO_EqInstance[SPIRIT_EQ_PERSIST_SIZE_IN_BYTES/4]  ;
TSpiritEQ_Band AUDIO_EQ_Bands[SPIRIT_EQ_MAX_BANDS];


__IO uint32_t AUDIO_LdCtrlPersistance[SPIRIT_LDCTRL_PERSIST_SIZE_IN_BYTES/4];
__IO uint32_t AUDIO_LdCtrlScratch[SPIRIT_LDCTRL_SCRATCH_SIZE_IN_BYTES/4];
TSpiritLdCtrl_Prms AUDIO_LdCtrlInstanceParams;
TSpiritEQ_Band *tmpEqBand;
#endif

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes audio
  * @param  None.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Init(uint8_t volume)
{
#if (!defined ( __GNUC__ ))  
  uint32_t index = 0;
  __IO uint32_t ldness_value;
#endif
  
   /* Try to Init Audio interface in diffrent config in case of failure */
  BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, volume, I2S_AUDIOFREQ_48K);
  BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
  
  /* Initialize internal audio structure */
  haudio.state  = AUDIOPLAYER_STOP;
  haudio.mute   = MUTE_OFF;
  haudio.volume = volume;  
  
#if (!defined ( __GNUC__ ))  
  /* Enable the Eq */
  SpiritEQ_Init((TSpiritEq *)AUDIO_EqInstance, I2S_AUDIOFREQ_48K);
  
  
  /* Retreive stored settings and set band params */
  SpiritEQ_FltGet((TSpiritEq *)AUDIO_EqInstance, &AUDIO_EQ_Bands[0], 0 );
  AUDIO_EQ_Bands[0].gainDb = k_BkupRestoreParameter(CALIBRATION_AUDIOPLAYER_EQU1_BKP);
  SET_BAND_PRMS(&AUDIO_EQ_Bands[0], SPIRIT_EQ_FLT_TYPE_SHELVING_LOWPASS , 0, 1000, AUDIO_EQ_Bands[0].gainDb);
  
  SpiritEQ_FltGet((TSpiritEq *)AUDIO_EqInstance, &AUDIO_EQ_Bands[1], 1 );
  AUDIO_EQ_Bands[1].gainDb = k_BkupRestoreParameter(CALIBRATION_AUDIOPLAYER_EQU2_BKP);    
  SET_BAND_PRMS(&AUDIO_EQ_Bands[1], SPIRIT_EQ_FLT_TYPE_PEAKING , 2000, 1000, AUDIO_EQ_Bands[1].gainDb);
  
  SpiritEQ_FltGet((TSpiritEq *)AUDIO_EqInstance, &AUDIO_EQ_Bands[2], 2 );
  AUDIO_EQ_Bands[2].gainDb = k_BkupRestoreParameter(CALIBRATION_AUDIOPLAYER_EQU3_BKP);   
  SET_BAND_PRMS(&AUDIO_EQ_Bands[2], SPIRIT_EQ_FLT_TYPE_PEAKING , 5000, 3000, AUDIO_EQ_Bands[2].gainDb);
  
  SpiritEQ_FltGet((TSpiritEq *)AUDIO_EqInstance, &AUDIO_EQ_Bands[3], 3 );
  AUDIO_EQ_Bands[3].gainDb = k_BkupRestoreParameter(CALIBRATION_AUDIOPLAYER_EQU4_BKP);;  
  SET_BAND_PRMS(&AUDIO_EQ_Bands[3], SPIRIT_EQ_FLT_TYPE_PEAKING , 10000, 6000, AUDIO_EQ_Bands[3].gainDb);
  
  SpiritEQ_FltGet((TSpiritEq *)AUDIO_EqInstance, &AUDIO_EQ_Bands[4], 4 );
  AUDIO_EQ_Bands[4].gainDb = k_BkupRestoreParameter(CALIBRATION_AUDIOPLAYER_EQU5_BKP);  
  SET_BAND_PRMS(&AUDIO_EQ_Bands[4], SPIRIT_EQ_FLT_TYPE_SHELVING_HIPASS , 15000, 2000, AUDIO_EQ_Bands[4].gainDb);

  for (index = 0; index < SPIRIT_EQ_MAX_BANDS ; index++)
  {
    tmpEqBand = &AUDIO_EQ_Bands[index];
    SpiritEQ_FltSet((TSpiritEq *)AUDIO_EqInstance, tmpEqBand, index);
  }
  
  /* Enable Loundness Control */
  SpiritLdCtrl_Init((TSpiritLdCtrl*)AUDIO_LdCtrlPersistance, I2S_AUDIOFREQ_48K);
  SpiritLdCtrl_GetPrms((TSpiritLdCtrl*)AUDIO_LdCtrlPersistance, &AUDIO_LdCtrlInstanceParams);
  ldness_value = k_BkupRestoreParameter(CALIBRATION_AUDIOPLAYER_LOUD_BKP);
  AUDIO_LdCtrlInstanceParams.gainQ8 = PERC_TO_LDNS_DB(ldness_value);
  SpiritLdCtrl_SetPrms((TSpiritLdCtrl*)AUDIO_LdCtrlPersistance, &AUDIO_LdCtrlInstanceParams);
#endif   
  
  /* Create Audio Queue */
  osMessageQDef(AUDIO_Queue, 1, uint16_t);
  AudioEvent = osMessageCreate (osMessageQ(AUDIO_Queue), NULL); 
  
  /* Create Audio task */
  osThreadDef(osAudio_Thread, Audio_Thread, osPriorityRealtime, 0, 5 * configMINIMAL_STACK_SIZE);
  AudioThreadId = osThreadCreate (osThread(osAudio_Thread), NULL);  

  return AUDIOPLAYER_ERROR_NONE;
}

#if (!defined ( __GNUC__ ))
/**
  * @brief  This function Set the new gain of the equilizer
  * @param  BandNum : equilizer band index
  * @param  NewGainValue : the new band gain.
  * @retval None.
*/
void AUDIO_SetEq(uint8_t BandNum, int16_t NewGainValue)
{
  /* GetBand */
  SpiritEQ_FltGet((TSpiritEq *)AUDIO_EqInstance, &AUDIO_EQ_Bands[BandNum], BandNum );
  
  /* SetNewValue */
  AUDIO_EQ_Bands[BandNum].gainDb = PERC_TO_EQUI_DB(NewGainValue);
  
  SpiritEQ_FltSet((TSpiritEq *)AUDIO_EqInstance, &AUDIO_EQ_Bands[BandNum], BandNum);
}


/**
  * @brief  This function Set Loudness Control Gain Value.
  * @param  NewGainValue in %.
  * @retval None.
  */
void AUDIO_SetLoudnessGain(int16_t NewGainValue)
{
  /* Get old Gain */
  SpiritLdCtrl_GetPrms((TSpiritLdCtrl*)AUDIO_LdCtrlPersistance, &AUDIO_LdCtrlInstanceParams);
  AUDIO_LdCtrlInstanceParams.gainQ8 = PERC_TO_LDNS_DB (NewGainValue);
  SpiritLdCtrl_Reset  ((TSpiritLdCtrl*)AUDIO_LdCtrlPersistance);
  SpiritLdCtrl_SetPrms((TSpiritLdCtrl*)AUDIO_LdCtrlPersistance, &AUDIO_LdCtrlInstanceParams);
}

/**
  * @brief  This function Set the new gain of the equilizer
  * @param  BandNum : equilizer band index
  * @param  NewGainValue : the new band gain.
  * @retval None.
*/
void AUDIO_SetEqParams(uint32_t loudness_perc)
{
  
  k_BkupSaveParameter(CALIBRATION_AUDIOPLAYER_EQU1_BKP, AUDIO_EQ_Bands[0].gainDb);
  k_BkupSaveParameter(CALIBRATION_AUDIOPLAYER_EQU2_BKP, AUDIO_EQ_Bands[1].gainDb);
  k_BkupSaveParameter(CALIBRATION_AUDIOPLAYER_EQU3_BKP, AUDIO_EQ_Bands[2].gainDb);
  k_BkupSaveParameter(CALIBRATION_AUDIOPLAYER_EQU4_BKP, AUDIO_EQ_Bands[3].gainDb);
  k_BkupSaveParameter(CALIBRATION_AUDIOPLAYER_EQU5_BKP, AUDIO_EQ_Bands[4].gainDb);
  
  k_BkupSaveParameter(CALIBRATION_AUDIOPLAYER_LOUD_BKP, loudness_perc);
}
#endif    

/**
  * @brief  Get audio state
  * @param  None.
  * @retval Audio state.
  */
AUDIOPLAYER_StateTypdef  AUDIOPLAYER_GetState(void)
{
  return haudio.state;
}

/**
  * @brief  Get audio volume
  * @param  None.
  * @retval Audio volume.
  */
uint32_t  AUDIOPLAYER_GetVolume(void)
{
  return haudio.volume;
}

/**
  * @brief  Set audio volume
  * @param  Volume: Volume level to be set in percentage from 0% to 100% (0 for 
  *         Mute and 100 for Max volume level).
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_SetVolume(uint32_t volume)
{
  if(BSP_AUDIO_OUT_SetVolume(volume) == AUDIO_OK)
  {
    haudio.volume = volume;
    return AUDIOPLAYER_ERROR_NONE;    
  }
  else
  {
    return AUDIOPLAYER_ERROR_HW;
  }
}

/**
  * @brief  Play audio stream
  * @param  frequency: Audio frequency used to play the audio stream.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Play(uint32_t frequency)
{
  uint32_t numOfReadBytes;
  haudio.state = AUDIOPLAYER_PLAY;
  
    /* Fill whole buffer @ first time */
    if(f_read(&wav_file, 
              &haudio.buffer[0], 
              AUDIO_BUFFER_SIZE, 
              (void *)&numOfReadBytes) == FR_OK)
    { 
      if(numOfReadBytes != 0)
      {
        BSP_AUDIO_OUT_SetFrequency(frequency);
        osThreadResume(AudioThreadId);
        BSP_AUDIO_OUT_Play((uint16_t*)&haudio.buffer[0], AUDIO_BUFFER_SIZE);   
        return AUDIOPLAYER_ERROR_NONE;
      }
    }
  return AUDIOPLAYER_ERROR_IO;

}

/**
  * @brief  Audio player process
  * @param  None.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Process(void)
{
  switch(haudio.state)
  {
  case AUDIOPLAYER_START:
    haudio.state = AUDIOPLAYER_PLAY;
    break;    

  case AUDIOPLAYER_EOF:
     haudio.state = AUDIOPLAYER_EOF;
     AUDIOPLAYER_NotifyEndOfFile();
    break;    
    
  case AUDIOPLAYER_ERROR:
     AUDIOPLAYER_Stop();
    break;
    
  case AUDIOPLAYER_STOP:
  case AUDIOPLAYER_PLAY:    
  default:
    break;
  }
  
  return AUDIOPLAYER_ERROR_NONE;
}

static void NOR_MspInit(void)
{
 GPIO_InitTypeDef GPIO_Init_Structure;
  
  /* Enable FMC clock */
  __HAL_RCC_FMC_CLK_ENABLE();

  /* Enable GPIOs clock */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  
  /* Common GPIO configuration */
  GPIO_Init_Structure.Mode      = GPIO_MODE_AF_PP;
  GPIO_Init_Structure.Pull      = GPIO_PULLUP;
  GPIO_Init_Structure.Speed     = GPIO_SPEED_HIGH;
  GPIO_Init_Structure.Alternate = GPIO_AF12_FMC;
  
  /* GPIOD configuration */
  GPIO_Init_Structure.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6   |\
                              GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |\
                              GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOD, &GPIO_Init_Structure);

  /* GPIOE configuration */  
  GPIO_Init_Structure.Pin   = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6   |\
                              GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |\
                              GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOE, &GPIO_Init_Structure);
  
  /* GPIOF configuration */  
  GPIO_Init_Structure.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2| GPIO_PIN_3 | GPIO_PIN_4     |\
                              GPIO_PIN_5 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOF, &GPIO_Init_Structure);
  
  /* GPIOG configuration */  
  GPIO_Init_Structure.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2| GPIO_PIN_3 | GPIO_PIN_4     |\
                              GPIO_PIN_5;
  HAL_GPIO_Init(GPIOG, &GPIO_Init_Structure); 
  
}
/**
  * @brief  Audio player DeInit
  * @param  None.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_DeInit(void)
{
  RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;
    

  haudio.state = AUDIOPLAYER_STOP; 
  
  __disable_irq();
  BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW);
  BSP_AUDIO_OUT_DeInit();
    __enable_irq();
  f_close(&wav_file); 
  
  vQueueDelete(AudioEvent);  
  osThreadTerminate(AudioThreadId);

  /* Restore SAI PLL clock */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 429;	
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 5;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_4;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct); 
  NOR_MspInit();
  return AUDIOPLAYER_ERROR_NONE;
}

/**
  * @brief  Stop audio stream.
  * @param  None.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Stop(void)
{
  haudio.state = AUDIOPLAYER_STOP;
  BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW); 
  
  if(AudioThreadId != 0)
  {  
    osThreadSuspend(AudioThreadId); 
  }
  f_close(&wav_file);


  return AUDIOPLAYER_ERROR_NONE;
}


/**
  * @brief  Pause Audio stream
  * @param  None.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Pause(void)
{
  if(AudioThreadId != 0)
  {
    osThreadSuspend(AudioThreadId); 
  }
  BSP_AUDIO_OUT_Pause();
  return AUDIOPLAYER_ERROR_NONE;
}


/**
  * @brief  Resume Audio stream
  * @param  None.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Resume(void)
{
  if(AudioThreadId != 0)
  {  
    osThreadResume(AudioThreadId);  
  }
  BSP_AUDIO_OUT_Resume();
  return AUDIOPLAYER_ERROR_NONE;
}
/**
  * @brief  Sets audio stream position
  * @param  position: stream position.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_SetPosition(uint32_t position)
{
  long file_pos;
  
  file_pos = wav_file.fsize / AUDIO_BUFFER_SIZE / 100; 
  file_pos *= (position * AUDIO_BUFFER_SIZE);

  f_lseek(&wav_file, file_pos);

  
  return AUDIOPLAYER_ERROR_NONE;
}

/**
  * @brief  Sets the volume at mute
  * @param  state: could be MUTE_ON to mute sound or MUTE_OFF to unmute 
  *                the codec and restore previous volume level.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_Mute(uint8_t state)
{
   BSP_AUDIO_OUT_SetMute(state);
   
   return AUDIOPLAYER_ERROR_NONE;
}

/**
  * @brief  Get the wav file information.
  * @param  file: wav file.
  * @param  info: pointer to wav file structure
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_GetFileInfo(char* file, WAV_InfoTypedef* info)
{
  uint32_t numOfReadBytes;
  AUDIOPLAYER_ErrorTypdef ret = AUDIOPLAYER_ERROR_IO;
  FIL fsfile;
  
  if( f_open(&fsfile, file, FA_OPEN_EXISTING | FA_READ) == FR_OK) 
  {
    /* Fill the buffer to Send */
    if(f_read(&fsfile, info, sizeof(WAV_InfoTypedef), (void *)&numOfReadBytes) == FR_OK)
    {
      if((info->ChunkID == 0x46464952) && (info->AudioFormat == 1))
      {
        ret = AUDIOPLAYER_ERROR_NONE;
      }
    }
    f_close(&fsfile);      
  }
  return ret;
}

/**
  * @brief  Select wav file.
  * @param  file: wav file.
  * @retval Audio state.
  */
AUDIOPLAYER_ErrorTypdef  AUDIOPLAYER_SelectFile(char* file)
{
  AUDIOPLAYER_ErrorTypdef ret = AUDIOPLAYER_ERROR_IO;
  if( f_open(&wav_file, file, FA_OPEN_EXISTING | FA_READ) == FR_OK) 
  {
    f_lseek(&wav_file, sizeof(WAV_InfoTypedef));
    ret = AUDIOPLAYER_ERROR_NONE;
  }
  return ret;  
}

/**
  * @brief  Get wav file progress.
  * @param  None
  * @retval file progress.
  */
uint32_t AUDIOPLAYER_GetProgress(void)
{
  return (wav_file.fptr);
}

/**
  * @brief  Manages the DMA Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
  if(haudio.state == AUDIOPLAYER_PLAY)
  {
    BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)&haudio.buffer[0], AUDIO_BUFFER_SIZE /2);
    osMessagePut ( AudioEvent, BUFFER_OFFSET_FULL, 0);    
  }
}

/**
  * @brief  Manages the DMA Half Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{ 
  if(haudio.state == AUDIOPLAYER_PLAY)
  {
    BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)&haudio.buffer[AUDIO_BUFFER_SIZE /2], AUDIO_BUFFER_SIZE /2);
    osMessagePut ( AudioEvent, BUFFER_OFFSET_HALF, 0);    
  }
}

/**
  * @brief  Manages the DMA FIFO error interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_Error_CallBack(void)
{
  haudio.state = AUDIOPLAYER_ERROR;
}

/**
  * @brief  Audio task
  * @param  argument: pointer that is passed to the thread function as start argument.
  * @retval None
  */
static void Audio_Thread(void const * argument)
{
  uint32_t numOfReadBytes;    
  osEvent event;  
  for(;;)
  {
    event = osMessageGet(AudioEvent, 100 );
    
    if( event.status == osEventMessage )
    {
      if(haudio.state == AUDIOPLAYER_PLAY)
      {
        switch(event.value.v)
        {
        case BUFFER_OFFSET_HALF:
          if(f_read(&wav_file, 
                    &haudio.buffer[0], 
                    AUDIO_BUFFER_SIZE/2, 
                    (void *)&numOfReadBytes) == FR_OK)
          { 
            if(numOfReadBytes == 0)
            {  
              haudio.state = AUDIOPLAYER_EOF;
            } 
            
#if (!defined ( __GNUC__ ))            
            SpiritEQ_Apply((void *)AUDIO_EqInstance,
                           /* NB_Channel */2, 
                           (int16_t *)&haudio.buffer[0], 
                           numOfReadBytes  / 4);
            
            /* Apply Loudness settings */
            SpiritLdCtrl_Apply((TSpiritLdCtrl *)AUDIO_LdCtrlPersistance,
                               /* NB_Channel */ 2, 
                               (int16_t *)&haudio.buffer[0], 
                               numOfReadBytes / 4, 
                               (void *)AUDIO_LdCtrlScratch);            
#endif            
            
          }
          else
          {
              haudio.state = AUDIOPLAYER_ERROR;    
          }
          break;  
          
        case BUFFER_OFFSET_FULL:
          if(f_read(&wav_file, 
                    &haudio.buffer[AUDIO_BUFFER_SIZE/2], 
                    AUDIO_BUFFER_SIZE/2, 
                    (void *)&numOfReadBytes) == FR_OK)
          { 
            if(numOfReadBytes == 0)
            { 
              haudio.state = AUDIOPLAYER_EOF;                     
            } 

#if (!defined ( __GNUC__ ))             
            SpiritEQ_Apply((void *)AUDIO_EqInstance,
                           /* NB_Channel */2, 
                           (int16_t *)&haudio.buffer[AUDIO_BUFFER_SIZE /2], 
                           numOfReadBytes  / 4);
            
            
            /* Apply Loudness settings */
            SpiritLdCtrl_Apply((TSpiritLdCtrl *)AUDIO_LdCtrlPersistance,
                               /* NB_Channel */ 2, 
                               (int16_t *)&haudio.buffer[AUDIO_BUFFER_SIZE /2], 
                               numOfReadBytes / 4, 
                               (void *)AUDIO_LdCtrlScratch);                
#endif            
          }
          else
          {
              haudio.state = AUDIOPLAYER_ERROR;   
          }
          break;   
          
        default:
          break;
        }
      }
      
    }
  }
}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
