#include <kwHeltecWifikit32.h>

char buf[10] = { 0 };
char timeString[8] = { 0 };

uint8_t col0 = 0;  // First value column
uint8_t col1 = 0;  // Last value column.

SSD1306AsciiWire oled;
WiFiClient       wifiClient;
WiFiMulti        wifiMulti;
WiFiUDP          Udp;

unsigned int      localPort = 8888;  // local port to listen for UDP packets
static const char ntpServerName[] = "us.pool.ntp.org";
const int         NTP_PACKET_SIZE = 48;
byte              packetBuffer[NTP_PACKET_SIZE];
const int         timeZone = 0;
time_t            lastDisplay = 0;  // when the digital clock was displayed
uint8_t           maxRows;
uint8_t           maxCols;

// PUBLIC
// ///////////////////////////////////////////////////////////////////////

// kwHeltecWifikit32 constructor
kwHeltecWifikit32::kwHeltecWifikit32( HeltecConfig config ) : config{ config }
{
  pinMode( LED, OUTPUT );
  getMacAddress();
  log_i( "Device ID: %s", deviceID );
}

// Initalise the display, WiFi, and MQTT
void kwHeltecWifikit32::init()
{
  Wire.begin( PIN_SDA, PIN_SCL );
  Wire.setClock( 400000L );

  initDisplay( config.rotateDisplay );
  updateSystemStatus( deviceID );
  setUpForm();

  initWiFi();
  initTime();
}

void kwHeltecWifikit32::initTime()
{
  Udp.begin( localPort );
  setSyncProvider( getNtpTime );
  setSyncInterval( 60 );
}

// Display the labeled data at the specified row
void kwHeltecWifikit32::setUpForm()
{
  oled.clear();
  // Setup form and find longest labels
  // for ( uint8_t i = 0; i < dataTopics.size(); ++i )
  // {
  //   const char* fieldName = dataTopics[i].fieldName.c_str();
  //   oled.println( fieldName );
  //   uint8_t w = oled.strWidth( fieldName );
  //   col0 = col0 < w ? w : col0;
  // }

  col0 += 3;
  col1 = col0 + oled.strWidth( "4000" ) + 2;

  // Print units
  // for ( uint8_t i = 0; i < dataTopics.size(); ++i )
  // {
  //   oled.setCursor( col1 + 1, i * oled.fontRows() );
  //   oled.print( dataTopics[i].units.c_str() );
  // }

  delay( 5000 );
}

// Initialise the Wifi - return true if successful
bool kwHeltecWifikit32::initWiFi()
{
  updateSystemStatus( "[->] WiFi" );

  char hostname[20];
  snprintf( hostname, 20, "heltec-%s", deviceID );

  WiFi.setHostname( hostname );

  wifiMulti.addAP( config.ap1.ssid, config.ap1.pwd );
  wifiMulti.addAP( config.ap2.ssid, config.ap2.pwd );
  wifiMulti.addAP( config.ap3.ssid, config.ap3.pwd );

  while ( wifiMulti.run() != WL_CONNECTED ) { ; }

  updateSystemStatus( "WiFi: connected" );
  delay( 500 );

  return WiFi.status() == WL_CONNECTED;
}

// Return true if it is midnight
bool kwHeltecWifikit32::isMidnight()
{
  return ( timeStatus() != timeNotSet )
             ? hour() == 0 && minute() == 0 && second() == 0
             : false;
}

// Publish

// void kwHeltecWifikit32::publish( uint8_t fieldID, uint16_t data )
// {
//   sprintf( buf, "%d", data );
//   const char* topic = dataTopics[fieldID].topicString.c_str();
//   mqttClient.publish( topic, buf );
// }

// void kwHeltecWifikit32::publish( uint8_t fieldID, float data )
// {
//   sprintf( buf, "%.1f", data );
//   const char* topic = dataTopics[fieldID].topicString.c_str();
//   mqttClient.publish( topic, buf );
// }

void clearValue( uint8_t row ) { oled.clear( col0, col1, row, row ); }

void kwHeltecWifikit32::update( uint8_t fieldID, uint16_t data )
{
  clearValue( fieldID * oled.fontRows() );
  oled.print( data, 1 );
}

void kwHeltecWifikit32::update( uint8_t fieldID, float data )
{
  clearValue( fieldID * oled.fontRows() );
  oled.print( data, 1 );
}

void kwHeltecWifikit32::update( uint8_t fieldID, const char *message )
{
  clearValue( fieldID * oled.fontRows() );
  oled.print( message );
}

// Display system status
void kwHeltecWifikit32::updateSystemStatus( std::string statusMessage )
{
  oled.setCursor( 0, 3 * oled.fontRows() ), oled.clearToEOL();
  oled.print( statusMessage.c_str() );
}

// Run - keep MQTT alive and process commands
void kwHeltecWifikit32::run()
{
  if ( timeStatus() != timeNotSet )
  {
    if ( now() != lastDisplay )
    {  // update the display only if time has changed
      char buf[12];
      lastDisplay = now();
      sprintf( buf, "%02d:%02d:%02d", hour(), minute(), second() );
      updateSystemStatus( buf );
    }
  } else
  {
    updateSystemStatus( "Time not set" );
  }
}

// PRIVATE
// ///////////////////////////////////////////////////////////////////////

// Populate the device ID
void kwHeltecWifikit32::getMacAddress()
{
  String theAddress = WiFi.macAddress();
  theAddress.replace( ":", "" );
  strcpy( deviceID, theAddress.c_str() );
}

time_t getNtpTime()
{
  IPAddress ntpServerIP;  // NTP server's ip address

  while ( Udp.parsePacket() > 0 )
    ;  // discard any previously received packets
  Serial.println( "Transmit NTP Request" );
  // get a random server from the pool
  WiFi.hostByName( ntpServerName, ntpServerIP );
  Serial.print( ntpServerName );
  Serial.print( ": " );
  Serial.println( ntpServerIP );
  sendNTPpacket( ntpServerIP );
  uint32_t beginWait = millis();
  while ( millis() - beginWait < 3000 )
  {
    int size = Udp.parsePacket();
    if ( size >= NTP_PACKET_SIZE )
    {
      Serial.println( "Receive NTP Response" );
      Udp.read( packetBuffer,
                NTP_PACKET_SIZE );  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println( "No NTP Response :-(" );
  return 0;  // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket( IPAddress &address )
{
  // set all bytes in the buffer to 0
  memset( packetBuffer, 0, NTP_PACKET_SIZE );
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket( address, 123 );  // NTP requests are to port 123
  Udp.write( packetBuffer, NTP_PACKET_SIZE );
  Udp.endPacket();
}

void initDisplay( bool isRotated = false )
{
  oled.begin( &Adafruit128x64, I2C_ADDRESS, PIN_RST );
  oled.setFont( Callibri15 );
  oled.setLetterSpacing( 2 );
  maxRows = oled.displayHeight() / ( oled.fontRows() * 8 );  // 4
  maxCols = oled.displayWidth() / oled.fontWidth();          // 13
  oled.displayRemap( isRotated );
}

void onWsEvent( AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len )
{
  switch ( type )
  {
    case WS_EVT_CONNECT:
      digitalWrite( LED, HIGH );
      log_i( "Websocket client connection received" );
      client->text( "Hello from ESP32 Server" );
      log_i( " Websocket client connection received " );
      break;

    case WS_EVT_DISCONNECT:
      digitalWrite( LED, LOW );
      log_i( " Websocket disconnected " );
      break;

    default: break;
  }
}