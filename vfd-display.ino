//hardware setup
#define PIN_INPUT 25
#define PIN_LOAD  27
#define PIN_CLOCK 26
#define PIN_BLANK 14

void setup() {
  Serial.begin(115200);
  pinMode(PIN_INPUT, OUTPUT);
  pinMode(PIN_LOAD,  OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_BLANK, OUTPUT);
}
//char to 8 seg ancoder
uint8_t chars[] = {
    0b00000000, //32 = space ( Space )
    0b10000010, //33 = ! ( Exclamation mark )
    0b00000001, //34 = " ( Double quotes ; Quotation mark ; speech marks )
    0b01100011, //35 = # ( Number sign )
    0b00010001, //36 = $ ( Dollar sign )
    0b01100011, //37 = % ( Percent sign )
    0b00000100, //38 = & ( Ampersand )
    0b00000010, //39 = ' ( Single quote or Apostrophe )
    0b01111001, //40 = ( ( round brackets or parentheses, opening round bracket )
    0b00001111, //41 = ) ( parentheses or round brackets, closing parentheses )
    0b00100000, //42 = * ( Asterisk )
    0b01000110, //43 = + ( Plus sign )
    0b10000000, //44 = , ( Comma )
    0b01000000, //45 = - ( Hyphen , minus sign )
    0b10000000, // 46 = . ( Dot, full stop )
    0b01010010, // 47 = / ( Slash , forward slash , fraction bar , division slash )
    0b00111111, // 48 = 0 ( number zero )
    0b00000110, // 49 = 1 ( number one )
    0b01011011, // 50 = 2 ( number two )
    0b01001111, // 51 = 3 ( number three )
    0b01100110, // 52 = 4 ( number four )
    0b01101101, // 53 = 5 ( number five )
    0b01111101, // 54 = 6 ( number six )
    0b00000111, // 55 = 7 ( number seven )
    0b01111111, // 56 = 8 ( number eight )
    0b01101111, // 57 = 9 ( number nine )
    0b01001000, // 58 = : ( Colon )
    0b01001000, // 59 = ; ( Semicolon )
    0b01011000, // 60 = < ( Less-than sign )
    0b01001000, // 61 = = ( Equals sign )
    0b01001100, // 62 = > ( Greater-than sign ; Inequality )
    0b10000011, // 63 = ? ( Question mark )
    0b01011111, // 64 = @ ( At sign )
    0b01110111, // 65 = A ( Capital letter A )
    0b01111100, // 66 = B ( Capital letter B )
    0b01111001, // 67 = C ( Capital letter C )
    0b01011110, // 68 = D ( Capital letter D )
    0b01111001, // 69 = E ( Capital letter E )
    0b01110001, // 70 = F ( Capital letter F )
    0b00111101, // 71 = G ( Capital letter G )
    0b01101110, // 72 = H ( Capital letter H )
    0b00000110, // 73 = I ( Capital letter I )
    0b00011110, // 74 = J ( Capital letter J )
    0b01111000, // 75 = K ( Capital letter K )
    0b00111000, // 76 = L ( Capital letter L )
    0b00010101, // 77 = M ( Capital letter M )
    0b01010100, // 78 = N ( Capital letter N )
    0b00111111, // 79 = O ( Capital letter O )
    0b01110011, // 80 = P ( Capital letter P )
    0b01100111, // 81 = Q ( Capital letter Q )
    0b01010000, // 82 = R ( Capital letter R )
    0b01101101, // 83 = S ( Capital letter S )
    0b01111000, // 84 = T ( Capital letter T )
    0b00111110, // 85 = U ( Capital letter U )
    0b00111110, // 86 = V ( Capital letter V )
    0b00101010, // 87 = W ( Capital letter W )
    0b01110110, // 88 = X ( Capital letter X )
    0b01101110, // 89 = Y ( Capital letter Y )
    0b00011011, // 90 = Z ( Capital letter Z )
    0b00111001, // 91 = [ ( square brackets or box brackets, opening bracket )
    0b00100100, // 92 = \ ( Backslash , reverse slash )
    0b00001111, // 93 = ] ( box brackets or square brackets, closing bracket )
    0b00100011, // 94 = ^ ( Circumflex accent or Caret )
    0b00001000, // 95 = _ ( underscore , understrike , underbar or low line )
    0b00100000, // 96 = ` ( Grave accent )
    0b01110111, // 97 = a ( Lowercase letter a , minuscule a )
    0b01111100, // 98 = b ( Lowercase letter b , minuscule b )
    0b01011000, // 99 = c ( Lowercase letter c , minuscule c )
    0b01011110, // 100 = d ( Lowercase letter d , minuscule d )
    0b01111001, // 101 = e ( Lowercase letter e , minuscule e )
    0b01110001, // 102 = f ( Lowercase letter f , minuscule f )
    0b00111101, // 103 = g ( Lowercase letter g , minuscule g )
    0b01110100, // 104 = h ( Lowercase letter h , minuscule h )
    0b00000110, // 105 = i ( Lowercase letter i , minuscule i )
    0b00011111, // 106 = j ( Lowercase letter j , minuscule j )
    0b01111000, // 107 = k ( Lowercase letter k , minuscule k )
    0b00111000, // 108 = l ( Lowercase letter l , minuscule l )
    0b00010101, // 109 = m ( Lowercase letter m , minuscule m )
    0b01010100, // 110 = n ( Lowercase letter n , minuscule n )
    0b01011100, // 111 = o ( Lowercase letter o , minuscule o )
    0b01110011, // 112 = p ( Lowercase letter p , minuscule p )
    0b01100111, // 113 = q ( Lowercase letter q , minuscule q )
    0b01010000, // 114 = r ( Lowercase letter r , minuscule r )
    0b01101101, // 115 = s ( Lowercase letter s , minuscule s )
    0b01111000, // 116 = t ( Lowercase letter t , minuscule t )
    0b00011100, // 117 = u ( Lowercase letter u , minuscule u )
    0b00011100, // 118 = v ( Lowercase letter v , minuscule v )
    0b00101010, // 119 = w ( Lowercase letter w , minuscule w )
    0b01110110, // 120 = x ( Lowercase letter x , minuscule x )
    0b01101110, // 121 = y ( Lowercase letter y , minuscule y )
    0b00011011, // 122 = z ( Lowercase letter z , minuscule z )
    0b00001001, // 123 = { ( braces or curly brackets, opening braces )
    0b00110000, // 124 = | ( vertical-bar, vbar, vertical line or vertical slash )
    0b00001001, // 125 = } ( curly brackets or braces, closing curly brackets )
    0b01000000  // 126 = ~ ( Tilde ; swung dash )
};
//communication
uint8_t dict(char c){
   if(c<32 || c>126)return 0;
   return chars[c-32];
} 
//communication
void send_to_vfd(bool * buffer){
   //data should be 2*20 bits thing as we are using 2 MAX6921 in series
   for(uint8_t i=0; i<40; i++) { 
      digitalWrite(PIN_INPUT, buffer[i]);
      digitalWrite(PIN_CLOCK, HIGH);
      digitalWrite(PIN_CLOCK, LOW); 
   }
   //flush
   digitalWrite(PIN_LOAD, HIGH);
   digitalWrite(PIN_LOAD, LOW);
} 

void display(uint8_t high_dig, uint8_t high_seg, uint8_t low_dig, uint8_t low_seg, uint8_t dop = 0){
  //buffer
  bool data[40];
  for(uint8_t i=0; i<8; i++) data[i]    = low_seg  & (1 << i);
  for(uint8_t i=0; i<8; i++) data[i+12] = low_dig  & (1 << i);
  for(uint8_t i=0; i<8; i++) data[i+20] = high_seg & (1 << i);
  for(uint8_t i=0; i<8; i++) data[i+32] = high_dig & (1 << i);

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
    Serial.println((uint8_t)dataString[0]);
    display(0x00,0x00,0x01,dict(dataString[0]),0x07);
  }
  
  delay(100);
}
