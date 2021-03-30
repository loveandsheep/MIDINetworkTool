#include <ArduinoOSC.h>
#include <MIDI.h>
#include <EEPROM.h>

#define PIN_RAW_INPUT 2

#define PIN_POT_A0 0
#define PIN_POT_A1 1

static const uint16_t DEBOUNCE_COUNT = 50;
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, MIDI);

//==================================初期設定項目
OscEtherServer receiver;
bool factoryReset = true;
uint8_t mac[] = {0xC0, 0xFF, 0xEE, 0x01, 0xC0, 0x1C};
byte myIp[4]      = {192, 168, 1, 201};//デバイスIPアドレス
byte hostbyte[4]  = {192, 168, 1, 101};//送信先IPアドレス
String host = "";
int recv_port = 12400;//受信ポート番号
int send_port = 12500;//送信ポート番号

void setup()
{
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOn();
  
  if (factoryReset)//EEPROMの初期設定
  {
    //myIp[0-3]:自分のIPアドレス 
    for (int i = 0;i < 4;i++) 
      EEPROM.write(i, myIp[i]);

    //hostbyte[4-7]:送信先のIPアドレス
    for (int i = 0;i < 4;i++)
      EEPROM.write(i+4, hostbyte[i]); 

    EEPROM.write(8, recv_port  >> 8);
    EEPROM.write(9, recv_port & 0xFF);
    EEPROM.write(10, send_port >> 8);
    EEPROM.write(11, send_port & 0xFF);
  }

  for (int i = 0;i < 4;i++)
  {
    myIp[i] = EEPROM.read(i);
    hostbyte[i] = EEPROM.read(i+4);
  }
  recv_port = (EEPROM.read(8) << 8) | EEPROM.read(9);
  send_port = (EEPROM.read(10)<< 8) | EEPROM.read(11);

  host = "";
  for (int i = 0;i < 4;i++)
  {
    if (i > 0) host += ".";
    host += String(hostbyte[i]);
  }

  Ethernet.begin(mac, IPAddress(myIp));
  receiver.begin(recv_port);

  pinMode(PIN_RAW_INPUT, INPUT_PULLUP);
}

void loop()
{
  if (receiver.parse())
  {
    const OscMessage* msg = receiver.message();
    if (msg->address() == F("/host"))//送信先アドレス
    {
      String m = "Set sending address => ";
      for (int i = 0;i < 4;i++)
      {
        EEPROM.write(i+4, msg->arg<int>(i));
        m += String(msg->arg<int>(i));
        m += ".";        
      }
      OscEther.send(host, send_port, F("/debug"), m);
    }
    
    if (msg->address() == F("/client"))
    {
      String m = "Set device address => ";
      for (int i = 0;i < 4;i++)
      {
        EEPROM.write(i, msg->arg<int>(i));
        m += String(msg->arg<int>(i));
        m += ".";
      }
      OscEther.send(host, send_port, F("/debug"), m);
    }

    if (msg->address() == F("/port/recv"))
    {
      String m = "Set receiving port => " + String(msg->arg<int>(0));
      EEPROM.write(8, msg->arg<int>(0) >> 8 & 0xFF);
      EEPROM.write(9, msg->arg<int>(0) & 0xFF); 
      OscEther.send(host, send_port, F("/debug"), m);
    }
    if (msg->address() == F("/port/send"))
    {
      String m = "Set sending port => " + String(msg->arg<int>(0));
      EEPROM.write(10, msg->arg<int>(0) >> 8 & 0xFF);
      EEPROM.write(11, msg->arg<int>(0) & 0xFF); 
      OscEther.send(host, send_port, F("/debug"), m);
    }
  }

  
  static uint8_t  ticks = 0;
  static uint8_t  old_ticks = 0;

  if(digitalRead(PIN_RAW_INPUT) == LOW)
  {
    byte input;
    if(Serial.available() != 0)
    {
      input = Serial.read();
    
      if(input & 0x80)
      {
        // Serial.println();
      }
      // Serial.print(input, HEX);
      // Serial.print(' ');
    }
  }
  else
  {
    // turn the crank...
    if (  MIDI.read())
    {
      if (MIDI.getData1() & 0x80 || MIDI.getData2() & 0x80) 
      { 
        MIDI.begin(MIDI_CHANNEL_OMNI);
        return;
      }

      switch (MIDI.getType())
      {
        case midi::NoteOff :
          {
            //Channel, Note, Velocity
            OscEther.send(host, send_port, F("/noteOff"), MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
          }
          break;
        case midi::NoteOn :
          {
            //Channel, Note, Velocity
            OscEther.send(host, send_port, F("/noteOn"), MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
          }
          break;
        case midi::AfterTouchPoly :
          {
            //Channel, Note, AT
            OscEther.send(host, send_port, F("/polyAT"), MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
          }
          break;
        case midi::ControlChange :
          {
            //Channel, Controller, Value
            OscEther.send(host, send_port, F("/cc"), MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());
          }
          break;
        case midi::ProgramChange :
          {
            //Channel, Program
            OscEther.send(host, send_port, F("/program"), MIDI.getChannel(), MIDI.getData1());
          }
          break;
        case midi::AfterTouchChannel :
          {
            //Channel, Program
            OscEther.send(host, send_port, F("/chanAT"), MIDI.getChannel(), MIDI.getData1());
          }
          break;
        case midi::PitchBend :
          {
            uint16_t val;
            val = MIDI.getData2() << 7 | MIDI.getData1();

            OscEther.send(host, send_port, F("/pitch"), MIDI.getChannel(), val);
          }
          break;
        case midi::SystemExclusive :
          {
            // Sysex is special.
            // could contain very long data...
            // the data bytes form the length of the message,
            // with data contained in array member
            uint16_t length;
            const uint8_t  * data_p;

            // Serial.print("SysEx, chan: ");
            // Serial.print(MIDI.getChannel());
            // length = MIDI.getSysExArrayLength();

            // Serial.print(" Data: 0x");
            // data_p = MIDI.getSysExArray();
            // for (uint16_t idx = 0; idx < length; idx++)
            // {
            //   Serial.print(data_p[idx], HEX);
            //   Serial.print(" 0x");
            // }
            // Serial.println();

  
          }
          break;
        case midi::TimeCodeQuarterFrame :
          {
            // MTC is also special...
            // 1 byte of data carries 3 bits of field info 
            //      and 4 bits of data (sent as MS and LS nybbles)
            // It takes 2 messages to send each TC field,
            OscEther.send(host, send_port, F("/timeCode"), MIDI.getData1() >> 4, MIDI.getData1() & 0x0F);
          }
          break;
        case midi::SongPosition :
          {
            // Data is the number of elapsed sixteenth notes into the song, set as 
            // 7 seven-bit values, LSB, then MSB.
            OscEther.send(host, send_port, F("/songPosition"), MIDI.getData2() << 7 | MIDI.getData1());
          }
          break;
        case midi::SongSelect :
          {
            OscEther.send(host, send_port, F("/songSelect"), MIDI.getData1());
          }
          break;
        case midi::TuneRequest :
          {
            OscEther.send(host, send_port, F("/tuneRequest"));
          }
          break;
        case midi::Clock :
          {
            ticks++;

            //Channel, Controller, Value
            OscEther.send(host, send_port, F("/clock"), ticks);
          }
          break;
        case midi::Start :
          {
            ticks = 0;
            OscEther.send(host, send_port, F("/start"));
          }
          break;
        case midi::Continue :
          {
            ticks = old_ticks;
            OscEther.send(host, send_port, F("/continue"));
          }
          break;
        case midi::Stop :
          {
            old_ticks = ticks;
            OscEther.send(host, send_port, F("/stop"));
          }
          break;
        case midi::ActiveSensing :
          {
            OscEther.send(host, send_port, F("/activeSense"));
          }
          break;
        case midi::SystemReset :
          {
            OscEther.send(host, send_port, F("/systemReset"));
            MIDI.begin(MIDI_CHANNEL_OMNI);
          }
          break;
        case midi::InvalidType :
          {
            OscEther.send(host, send_port, F("/invalid Type"));
          }
          break;
        default:
          {
            // Serial.println();
          }
          break;
      }
    }
  }
}
