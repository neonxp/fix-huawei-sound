/*
* File: huawei_matebook14s_codec_fix.cpp
* Author: ursul_polar
*
* Created on Jan 22, 2023, 5:34 AM
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/io.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define HDA_VERB(nid,verb,param) ((nid)<<24 | (verb)<<8 | (param))
#define HDA_IOCTL_VERB_WRITE _IOWR('H', 0x11, struct hda_verb_ioctl)
#define GET_VARIABLE_NAME(Variable) (void(Variable),#Variable)

// Struct to contain HDA_VERBS properties
struct hda_verb_ioctl {
  u32 verb; /* HDA_VERB() */
  u32 res; /* response */
}state_check;

// Derivate struct to handle conn and EAPD separately
struct verbs_enable{
  struct hda_verb_ioctl conn;
  struct hda_verb_ioctl eapd;
}speaker_enable, headphone_enable;

bool done = false;

// If TERM Signal is sent will cause the loop to exit
void trpsig(int)
{
  done = true;
}

int get_conn_state(int fd){
  // Set standard verb to get status for HP connection
  state_check.verb = HDA_VERB(0x16, 0x0f09, 0x0);

  // Check connection status
  if (ioctl(fd, HDA_IOCTL_VERB_WRITE, &state_check) < 0)
    syslog(LOG_ERR, "Failed to read connection state verb.");
  return state_check.res >> 28;
}

int get_snd_device(){
  int fd;
  
  // Check that we can RW with regards to the sound card
  fd = open("/dev/snd/hwC0D0", O_RDWR|O_NOCTTY);
  
  if (fd < 0) {
    syslog(LOG_ERR, "Failed to open snd device.");
    exit(EXIT_FAILURE);
  }
  
  return fd;
}

void clear_pin(int fd, const char* type){
  struct hda_verb_ioctl clear_pins;

  int array[]={0x715, 0x716, 0x717};

  for(int i=0; i<3; i++){

    if(type == "headphone" && array[i] != 0x715)  
      clear_pins.verb = HDA_VERB(0x1, array[i], 0x2);
    else if( type =="headphone" && array[i] == 0x715)
      clear_pins.verb = HDA_VERB(0x1, array[i], 0x0);

    if(type == "spkr" && array[i] == 0x715)  
      clear_pins.verb = HDA_VERB(0x1, array[i], 0x2);
    else if( type =="spkr" && array[i] != 0x715)
      continue;
    
    if (clear_pins.verb > 0){
      if (ioctl(fd, HDA_IOCTL_VERB_WRITE, &clear_pins) < 0)
        syslog(LOG_ERR, "Failed to write clear pin hda verb.");
      else
        syslog(LOG_INFO, "Written data to hda verb %.8x", clear_pins.verb);  
    }
  }
  
}

void enable_verb(int fd, struct hda_verb_ioctl conn, struct hda_verb_ioctl eapd, const char* type){

  // Enable speakers with hda verbs
  if (ioctl(fd, HDA_IOCTL_VERB_WRITE, &conn) < 0)
    syslog(LOG_ERR, "Failed to write connection slector hda verb.");
  if (ioctl(fd, HDA_IOCTL_VERB_WRITE, &eapd) < 0)
    syslog(LOG_ERR, "Failed to write EAPD hda verb.");

  clear_pin(fd, type);
}

int main(int argc, char** argv) {
  // TERM Signal check
  signal(SIGTERM, &trpsig);

  // Sound card file descriptor and connection state vars
  int fd, state;

  // Check connection status
  state = 0;
  
  // Set standard verbs to enable connections and EAPD for speaker
  speaker_enable.conn.verb = HDA_VERB(0x16, 0x701, 0x0001);
  speaker_enable.eapd.verb = HDA_VERB(0x17, 0x70c, 0x0002);
  
  // Set standard verbs to enable connections and EAPD for headphones
  headphone_enable.conn.verb = HDA_VERB(0x16, 0x701, 0x0000);
  headphone_enable.eapd.verb = HDA_VERB(0x17, 0x70c, 0x0000);

  // Write to syslog
  syslog(LOG_INFO, "Daemon started.");

  // Check that we can RW with regards to the sound card
  fd = get_snd_device();

  // Enable Speaker on startup so that there's no need to have a audio jack inserted 
  // to trigger the switch
  enable_verb(fd, speaker_enable.conn, speaker_enable.eapd, "spkr");
    syslog(LOG_INFO, "Enabled speaker output");

  // Enter loop to detect connection changes
  while (!done)
  { 
    sleep(1);

    if (state != get_conn_state(fd) && state == 8){
      //Enable speaker
      enable_verb(fd, speaker_enable.conn, speaker_enable.eapd, "spkr");
      syslog(LOG_INFO, "Enabled speaker output");
    }
    
    if(state != get_conn_state(fd) && state == 0){
      //Enable headphones
      enable_verb(fd, headphone_enable.conn, headphone_enable.eapd, "headphone");
      syslog(LOG_INFO, "Enabled headphone output");
    }

    state = get_conn_state(fd);
  }
  // Close and exit
  close(fd);
  syslog(LOG_INFO, "Daemon stopped.");
  exit(EXIT_SUCCESS);
} 
