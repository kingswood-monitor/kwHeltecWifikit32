#include <kwHeltecWifikit32.h>
#include <kwNeoTimer.h>

char buf[10] = { 0 };
char timeString[8] = { 0 };

uint8_t col0 = 0;  // First value column
uint8_t col1 = 0;  // Last value column.

SSD1306AsciiWire oled;
WiFiClient       wifiClient;
PubSubClient     mqttClient( wifiClient );
RTC_DS1307       ds1307;

kwNeoTimer updateTimeTimer = kwNeoTimer();

// PUBLIC
// ///////////////////////////////////////////////////////////////////////

// kwHeltecWifikit32 constructor
kwHeltecWifikit32::kwHeltecWifikit32( HeltecConfig config ) : config{ config }
{
  getMacAddress();
  statusTopicID = registerMetaTopic( "status" );
  pinMode( LED, OUTPUT );
}

// Initalise the display, WiFi, and MQTT
void kwHeltecWifikit32::init()
{
  Wire.begin( PIN_SDA, PIN_SCL );
  Wire.setClock( 400000L );

  hasRTC = ds1307.begin( &Wire );
  if ( hasRTC && !ds1307.isrunning() )
  {
    ds1307.adjust( DateTime( F( __DATE__ ), F( __TIME__ ) ) );
    rtcWasAdjusted = true;
  }

  oled.begin( &Adafruit128x64, I2C_ADDRESS, PIN_RST );
  oled.setFont( Callibri15 );
  oled.setLetterSpacing( 2 );
  maxRows = oled.displayHeight() / ( oled.fontRows() * 8 );  // 4
  maxCols = oled.displayWidth() / oled.fontWidth();          // 13
  oled.displayRemap( config.rotateDisplay );

  // topicRoot = config.topicRoot;

  setUpForm();

  updateSystemStatus( deviceID );
  delay( 5000 );

  initWiFi( config.ssid, config.pwd );
  initMTTQ( config.mqtt_host );

  updateTimeTimer.set( 1000 );
}

// Display the labeled data at the specified row
void kwHeltecWifikit32::setUpForm()
{
  oled.clear();
  // Setup form and find longest labels
  for ( uint8_t i = 0; i < dataTopics.size(); ++i )
  {
    const char* fieldName = dataTopics[i].fieldName.c_str();
    oled.println( fieldName );
    uint8_t w = oled.strWidth( fieldName );
    col0 = col0 < w ? w : col0;
  }

  col0 += 3;
  col1 = col0 + oled.strWidth( "4000" ) + 2;

  // Print units
  for ( uint8_t i = 0; i < dataTopics.size(); ++i )
  {
    oled.setCursor( col1 + 1, i * oled.fontRows() );
    oled.print( dataTopics[i].units.c_str() );
  }
}

// Register data topic
uint8_t kwHeltecWifikit32::registerField( std::string fieldName,
                                          std::string units,
                                          std::string topicName,
                                          std::string sensorName )
{
  std::string topicString = config.topicRoot + "/" + "data" + "/" + topicName +
                            "/" + sensorName + "/" + deviceID;
  dataTopics.push_back( dataField{ topicString, fieldName, units } );

  return dataTopics.size() - 1;
}

// register meta topic
uint8_t kwHeltecWifikit32::registerMetaTopic( std::string topicName )
{
  std::string topicString =
      config.topicRoot + "/" + "meta" + "/" + topicName + "/" + deviceID;
  metaTopics.push_back( topicString );

  return metaTopics.size() - 1;
}

// Initialise the Wifi - return true if successful
bool kwHeltecWifikit32::initWiFi( const char* wifi_ssid, const char* wifi_pwd )
{
  updateSystemStatus( "[->] WiFi" );

  WiFi.disconnect();
  WiFi.mode( WIFI_MODE_STA );
  WiFi.begin( wifi_ssid );

  while ( WiFi.status() != WL_CONNECTED ) { delay( 500 ); };

  updateSystemStatus( "WiFi OK" );
  delay( 500 );

  return WiFi.status() == WL_CONNECTED;
}

// Initialise MTTQ
void kwHeltecWifikit32::initMTTQ( IPAddress mqtt_host )
{
  mqttClient.setServer( mqtt_host, 1883 );
  mqttClient.setCallback( mqttCallback );
  didInitialiseMTTQ = true;
}

// Return true if it is midnight
bool kwHeltecWifikit32::isMidnight()
{
  if ( hasRTC )
  {
    DateTime now = ds1307.now();
    return now.hour() == 0 && now.minute() == 0 && now.second() == 0;
  } else
  {
    return false;
  }
}

// Publish

void kwHeltecWifikit32::publish( uint8_t fieldID, uint16_t data )
{
  sprintf( buf, "%d", data );
  const char* topic = dataTopics[fieldID].topicString.c_str();
  mqttClient.publish( topic, buf );
}

void kwHeltecWifikit32::publish( uint8_t fieldID, float data )
{
  sprintf( buf, "%.1f", data );
  const char* topic = dataTopics[fieldID].topicString.c_str();
  mqttClient.publish( topic, buf );
}

// void clearValue(uint8_t row) {oled.clear(col0, col1, row, row + row - 1); }
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

void kwHeltecWifikit32::update( uint8_t fieldID, const char* message )
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
  if ( didInitialiseMTTQ )
  {
    if ( !mqttClient.connected() )
    {
      digitalWrite( LED, LOW );

      long now = millis();
      if ( now - lastReconnectAttempt > MQTT_RECONNECT_TIME_SECONDS * 1000 )
      {
        lastReconnectAttempt = now;
        updateSystemStatus( "[->] MQTT" );

        if ( mqttReconnect() ) { lastReconnectAttempt = millis(); }
      } else
      {
      }  // Wait for timer to expire
    } else
    {
      mqttClient.loop();
    }
  }

  // Update clock
  if ( updateTimeTimer.repeat() )
  {
    DateTime now = ds1307.now();
    char     buf[12];
    sprintf( buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second() );
    updateSystemStatus( buf );
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

// Rconnect MQTT
boolean kwHeltecWifikit32::mqttReconnect()
{
  updateSystemStatus( "[->] MQTT" );

  const char* statusTopic = metaTopics[statusTopicID].c_str();

  if ( mqttClient.connect( deviceID, statusTopic, 2, true, "OFFLINE" ) )
  {
    updateSystemStatus( "ONLINE" );
    digitalWrite( LED, HIGH );

    mqttClient.publish( statusTopic, "ONLINE" );
    lastReconnectAttempt = millis();
  }

  return mqttClient.connected();
}

// MQTT callback function
void kwHeltecWifikit32::mqttCallback( char* topic, byte* payload,
                                      unsigned int length )
{
}
