//hardware setup
#define PIN_INPUT 25
#define PIN_LOAD  27
#define PIN_CLOCK 26
#define PIN_BLANK 14

/*
0 - 19
1 - 23
2 - 
*/
//buffer
bool data[40];
//communication
void send_to_vfd(bool * buffer){
   //data should be 2*20 bits thing as we using 2 MAX6921 in series
   digitalWrite(PIN_BLANK, LOW); 
   for(uint8_t i=0; i<40; i++) 
   { 
      digitalWrite(PIN_INPUT, buffer[i]);
      digitalWrite(PIN_CLOCK, HIGH);
      digitalWrite(PIN_CLOCK, LOW); 
   }
   //flush
   digitalWrite(PIN_LOAD, HIGH);
   digitalWrite(PIN_LOAD, LOW);
} 
void setup() {
  Serial.begin(115200);
  pinMode(PIN_INPUT, OUTPUT);
  pinMode(PIN_LOAD,  OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_BLANK, OUTPUT);
}
void int_to_data(uint64_t num){
  for(uint8_t i=0; i<40; i++) {
    data[i] = num & (1 << i); 
  }
}
void display(uint8_t high_dig, uint8_t high_seg, uint8_t low_dig, uint8_t low_seg, uint8_t dop){

  for(uint8_t i=0; i<8; i++)data[i] = low_seg  & (1 << i);
  for(i=12; i<20; i++)      data[i] = low_dig  & (1 << i);
  for(i=20; i<28; i++)      data[i] = high_seg & (1 << i);
  for(i=32; i<40; i++)      data[i] = high_dig & (1 << i);

  //additional 17th segment
  data[29] = dop !=0;
  data[25] = dop & 1; //lower section
  data[26] = dop & 2; //midle section
  data[28] = dop & 4; //higher section //there is minor bug in hardware wiring so its not 27

  send_to_vfd(data);
  
}
void loop() {
  if (Serial.available()) {
   String dataString = Serial.readString(); // Read the incoming data as a String
    uint64_t incoming = atoi(dataString.c_str()); // Convert the String data to a uint64_t integer
    
      if(incoming!=0){
        int_to_data(incoming);
      Serial.println(incoming);
    }
  }
  send_to_vfd(data);
  delay(100);
}
