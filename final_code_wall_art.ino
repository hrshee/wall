#include <Servo.h>
#include <AccelStepper.h>


Servo penServo;
AccelStepper leftStepper(AccelStepper::FULL4WIRE, 3, 4, 5, 6); //Ldir1_pin1=3,Ldir2_pin=5
AccelStepper rightStepper(AccelStepper::FULL4WIRE, 30, 31, 32, 33); //Rdir1_pin1=30,Rdir2_pin=32

int stepsPerMM=20;  //steps required to increase length by 1mm of string
int L=100; //horizantal length of the picture we want to draw

#define LINE_BUFFER_LENGTH 512
boolean verbose=false;

struct point{
    float x, y, z;
};

struct point actuatorPos;

int penzUp=180, penzDown=0;
float Lpos, Rpos;

void setup(){
    Serial.begin(9600);
    Serial.println("Bot is ready");
    penServo.attach(10);//servo_pin=10
    penServo.write(penzUp);
    leftStepper.setMaxSpeed(100);
    leftStepper.setAcceleration(20);
    rightStepper.setMaxSpeed(100);
    rightStepper.setAcceleration(20);
    pinMode(2,OUTPUT);//Lpwm_pin1=2 
    pinMode(7,OUTPUT);//Lpwm_pin2=7

  digitalWrite(2, HIGH);
  digitalWrite(7, HIGH);
   pinMode(34,OUTPUT);//Rpwm_pin1=34
  pinMode(35,OUTPUT);//Rpwm_pin2=35

  digitalWrite(34, HIGH);
  digitalWrite(35, HIGH);
}

void loop(){

    delay(100);
    char line[ LINE_BUFFER_LENGTH ]; //creating char array to store line;
    char c;
    int lineIndex;
    bool lineIsComment, lineSemiColon;
    
    lineIndex = 0;
    lineSemiColon = false;
    lineIsComment = false;

    while (1) {

        // Serial reception - Mostly from Grbl, added semicolon support
        while ( Serial.available() > 0 ) {
            c = Serial.read();
            if (( c == '\n') || (c == '\r') ) {             // End of line reached
                if ( lineIndex > 0 ) {                        // Line is complete. Then execute!
                    line[ lineIndex ] = '\0';                   // Terminate string
                    if (verbose) {
                        Serial.print( "Received : ");
                        Serial.println( line );
                    }
                    processIncomingLine( line, lineIndex );
                    lineIndex = 0;
                }
                else {
                    // Empty or comment line. Skip block.
                }
                lineIsComment = false;
                lineSemiColon = false;
                Serial.print("ok    ");
                Serial.print("Line Recieved");
                Serial.println(line);
            } 
            else {
                if ( (lineIsComment) || (lineSemiColon) ) {   // Throw away all comment characters
                    if ( c == ')' )  lineIsComment = false;     // End of comment. Resume line.
                }
                else {
                    if ( c <= ' ' ) { }                          // Throw away whitepace and control characters
                    else if ( c == '/' ) { }                   // Block delete not supported. Ignore character.         
                    else if ( c == '(' ) {                    // Enable comments flag and ignore all characters until ')' or EOL.
                        lineIsComment = true;
                    }
                    else if ( c == ';' ) {
                        lineSemiColon = true;
                    }
                    else if ( lineIndex >= LINE_BUFFER_LENGTH - 1 ) {
                        Serial.println( "ERROR - lineBuffer overflow" );
                        lineIsComment = false;
                        lineSemiColon = false;
                    }
                    else if ( c >= 'a' && c <= 'z' ) {        // Upcase lowercase
                        line[ lineIndex++ ] = c - 'a' + 'A';
                    }
                    else {
                        line[ lineIndex++ ] = c;
                    }
                }
            }
        }
    }
}

void processIncomingLine( char* line, int charNB ) {
    int currentIndex = 0;
    char buffer[ 64 ];                                 // Hope that 64 is enough for 1 parameter
    struct point newPos;

    newPos.x = 0.0;
    newPos.y = 0.0;

    //  Needs to interpret
    //  G1 for moving
    //  G4 P300 (wait 150ms)
    //  G1 X60 Y30
    //  G1 X30 Y50
    //  M300 S30 (pen down)
    //  M300 S50 (pen up)
    //  Discard anything with a (
    //  Discard any other command!

    while ( currentIndex < charNB ) {
        switch ( line[ currentIndex++ ] ) {              // Select command, if any
        
        case 'U':
            penUp();
            break;
        
        case 'D':
            penDown();
            break;
        
        case 'G':
            buffer[0] = line[ currentIndex++ ];          // /!\ Dirty - Only works with 2 digit commands
            //      buffer[1] = line[ currentIndex++ ];
            //      buffer[2] = '\0';
            buffer[1] = '\0';
  
            switch ( atoi( buffer ) ) {                  // Select G command
                case 0:                                   // G00 & G01 - Movement or fast movement. Same here
                case 1:
                // /!\ Dirty - Suppose that X is before Y
                char* indexX = strchr( line + currentIndex, 'X' ); // Get X/Y position in the string (if any)
                char* indexY = strchr( line + currentIndex, 'Y' );
                if ( indexY <= 0 ) {
                    newPos.x = atof( indexX + 1);
                    newPos.y = actuatorPos.y;
                }
                else if ( indexX <= 0 ) {
                    newPos.y = atof( indexY + 1);
                    newPos.x = actuatorPos.x;
                }
                else {
                    newPos.y = atof( indexY + 1);
                    indexY = '\0';
                    newPos.x = atof( indexX + 1);
                }
                drawLine(newPos.x, newPos.y );
                //        Serial.println("ok");
                actuatorPos.x = newPos.x;
                actuatorPos.y = newPos.y;
                break;
            }
            break;
        case 'M':
            buffer[0] = line[ currentIndex++ ];        // /!\ Dirty - Only works with 3 digit commands
            buffer[1] = line[ currentIndex++ ];
            buffer[2] = line[ currentIndex++ ];
            buffer[3] = '\0';
            switch ( atoi( buffer ) ) {
                case 300:{
                    char* indexS = strchr( line + currentIndex, 'S' );
                    float Spos = atof( indexS + 1);
                    //         Serial.println("ok");
                    if (Spos == 30) {
                        penDown();
                    }
                    if (Spos == 50) {
                        penUp();
                    }
                    break;
                }
                case 114:                                // M114 - Repport position
                    Serial.print( "Absolute position : X = " );
                    Serial.print( actuatorPos.x );
                    Serial.print( "  -  Y = " );
                    Serial.println( actuatorPos.y );
                    break;
                default:
                    Serial.print( "Command not recognized : M");
                    Serial.println( buffer );
            } 
        }
    }
}


void drawLine(float x1, float y1){
    float l1=stepsPerMM*leftLength(x1, y1);
    float r1=-stepsPerMM*rightLength(x1, y1);

    leftStepper.moveTo(l1);
    rightStepper.moveTo(r1);

    leftStepper.setSpeed(abs(l1-Lpos));
    rightStepper.setSpeed(abs(r1-Rpos));

    while(leftStepper.distanceToGo()>0||rightStepper.distanceToGo()>0){
        leftStepper.run();
        rightStepper.run();
        delay(5);
    }
    Serial.println("Done!");

    Lpos=l1;
    Rpos=r1;
}

void penUp(){
    delay(100);
    penServo.write(penzUp);
    delay(100);
    actuatorPos.z=1;
}

void penDown(){
    delay(100);
    penServo.write(penzDown);
    delay(100);
    actuatorPos.z=0;
}


float leftLength(float x, float y){
    return sqrt(x*x+y*y);
}

float rightLength(float x, float y){
    return sqrt((L-x)*(L-x)+y*y);
}
