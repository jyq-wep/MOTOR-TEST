#include<WiFi.h>
#include<WiFiClient.h>
#include<PubSubClient.h>
#define STBY_PIN 4
#define AIN1_PIN 5
#define AIN2_PIN 6
#define MONITOR_SPEED 115200
#define DEVICE_ID "14e09200-ec3c-4331-ba74-5ca3a254191a"//设备密码
#define CMD_TOPIC "/CIA/IoT/" DEVICE_ID "/Command"//要订阅的指令主题
#define DATA_TOPIC "/CIA/IoT/Data"//回执发送的主题
const char* ssid="vivo Y77";
const char* password="87654321";
const char* server_host="47.113.229.47";//MQTT服务器
const int server_port=1883;//MQTT标准端口
WiFiClient client;
PubSubClient mqtt(client);
String ackMsg="";
int motorState=0;
void connectwifi(){
  if(WiFi.status()==WL_CONNECTED) return;
  int counter=0;
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED&&counter<20){
    delay(500);
    Serial.print(".");
    counter++;
  }
  Serial.println();
  if(WiFi.status()!=WL_CONNECTED){
    Serial.println("Connection failed, retry later");
    delay(1000);
    return;
  }
  Serial.println("Connection is successful");
  Serial.print("Current IP is ");
  Serial.println(WiFi.localIP());
}
String jsonStr(const String& s,const char* key){
  int k=s.indexOf(String("\"")+key+"\"");//找 "键名"
  if(k<0) return "";
  int c=s.indexOf(':',k);//键名后面跟着冒号
  if(c<0) return "";
  int q1=s.indexOf('"',c+1);//冒号后第一个引号是值的起始引号，容忍冒号后的空格
  if(q1<0) return "";
  int q2=s.indexOf('"',q1+1);//下一个引号是值的结束引号
  if(q2<0) return "";
  return s.substring(q1+1,q2);//取两个引号之间的内容
}
void onMessage(char* topic,byte* payload,unsigned int len){
  String rec;
  for(unsigned int i=0;i<len;i++) rec+=(char)payload[i];
  Serial.println(rec);

  //整包里带powerOn就是开，带powerOff就是关
  String status="error";
  if(rec.indexOf("3powerOn")>=0){motorState=1;status="ok";}
  else if(rec.indexOf("3powerOff")>=0){motorState=0;status="ok";}

  //回执，id原样带回去，平台靠它对上指令
  ackMsg="{\"type\":\"directive\",\"id\":\""+jsonStr(rec,"id")+"\",\"message\":\""+
         (status=="ok"?"执行完成":"未知指令")+"\",\"status\":\""+status+"\"}";
}
//连MQTT服务器并订阅指令主题
void connectmqtt(){
  if(mqtt.connect(DEVICE_ID,DEVICE_ID,NULL)){ //clientid=username=设备密码
    if(mqtt.subscribe(CMD_TOPIC)){//订阅后平台才会下发指令
      Serial.print("MQTT connected, subscribed OK ");
      Serial.println(CMD_TOPIC);
    }else{
      Serial.println("MQTT connected, but SUBSCRIBE FAILED");
    }
  }else{
    Serial.print("MQTT failed, rc=");
    Serial.println(mqtt.state());
  }
}
int readMonitor(){
  String rec;
  if(Serial.available()>0){
    rec=Serial.readStringUntil('\n');
    rec.trim();                 //去掉回车符
    if(rec=="on") return 1;
    if(rec=="off") return 0;
  }
  return -1;                    //没收到命令
}
void judge(){
  if(motorState==0){
    digitalWrite(AIN1_PIN,HIGH);
    digitalWrite(AIN2_PIN,HIGH);
  }
  else if(motorState==1){
    digitalWrite(AIN1_PIN,HIGH);
    digitalWrite(AIN2_PIN,LOW);
  }
  else return;
}
void setup() {
  Serial.begin(MONITOR_SPEED);
  delay(200);
  mqtt.setServer(server_host,server_port);//指定服务器地址和端口
  mqtt.setCallback(onMessage);//绑定回调，收到指令就进onMessage
  connectwifi();
  connectmqtt();
  pinMode(STBY_PIN,OUTPUT);
  pinMode(AIN1_PIN,OUTPUT);
  pinMode(AIN2_PIN,OUTPUT);
  digitalWrite(STBY_PIN,HIGH);
  digitalWrite(AIN1_PIN,LOW);
  digitalWrite(AIN2_PIN,LOW);
  Serial.println("Preparation is complete");
}
void loop() {
  connectwifi();
  mqtt.loop();
  if(ackMsg.length()>0)
  {
    mqtt.publish(DATA_TOPIC,ackMsg.c_str());
    Serial.print("Ack sent: ");
    Serial.println(ackMsg);
    ackMsg="";
  }
  int cmd=readMonitor();
  if(cmd>=0){
    motorState=cmd;
    Serial.println(motorState?"Switch on":"Switch off");
  }
  judge();
  delay(100);
}