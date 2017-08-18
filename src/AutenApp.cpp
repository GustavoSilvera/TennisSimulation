#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <fstream>
#include <string>
#include <iostream>

using namespace ci;
using namespace ci::app;
using namespace std;

#define Player1  true
#define Player2  false
#define singles  true
#define doubles  false
#define PI 3.14159265

//globals:
double TIME = 0;
double realTime;

double pixelsPerMeter = 38.284;//( ( ( 0.825 * 1400 ) - ( 0.175 * 1400 ) ) * ( ( 0.825 * 1400 ) - ( 0.175 * 1400 ) ) / ( 23.77 ) );

double sqr(double x) { return x*x; }

ofstream DATA("DATA.txt");

struct vec3 {
  vec3() : X(0.0), Y(0.0), Z(0.0) {}
  vec3(double x, double y, double z) : X(x), Y(y), Z(z) {}
  double X, Y, Z;
  
  vec3 times(double f) {
    return vec3(X*f, Y*f, Z*f);
  }
  double distance(vec3 v) {
    return sqrt(sqr(v.X - X) + sqr(v.Y - Y) + sqr(v.Z - Z));
  }
  vec3 operator+(vec3 v) {
    return vec3(X + v.X, Y + v.Y, Z + v.Z);
  }
};
vec3 gravity(0, 0, -9.8);

int playPoint = 0;//how many points were made

struct game {
  bool turn;
  bool startGame = false;
  struct ball {
    vec3 position;//X, Y, and Z
    /*double X;
      double Y;
      double Z;*/
    vec3 reference;//reference point, specific corners of the court for the ball
    /*
      (0, 1)|————(net)————|(1, 1)
      |      |      |
      |      |      |
      |—————————————|(service line)
      |             |
      |      |      |
      |——————p——————|
      (0, 0)        (1, 0)
    */
    double relativeX;
    double relativeY;
    vec3 speed;//diff x, diff y, diff z
    vec3 acceleration;//diff in diff x, diff in diff in y, diff in diff in z
    int bounceCount = 0;//amount of bounces the ball has done, for double bounces
    bool waitingForResult = false;//if the shot was just hit, but hasent reached legality yet
    ball(vec3 p, vec3 s, vec3 a, vec3 r) : position(p), speed(s), acceleration(a), reference(r) {}
    bool advance(double dt) {
      position = position + speed.times(dt);
      if (position.X - reference.X != 0){
	relativeX = speed.X * (position.X - reference.X);//gets the relative position of each player RELATIVE to the left back cornet of their blue section
      }
      else {
	relativeX = 0;//gets the relative position of each player RELATIVE to the left back cornet of their blue section
      }
      relativeY = speed.X * (position.Y - reference.Y);
      
      if (position.Z > 0) { speed = speed + acceleration.times(dt); }//only apply gravity when you arent already on the floor
      if (position.Z <= 0) {//if bounce, change speed to go up again.
	bounceCount++;
	position.Z = -position.Z;
	speed.Z = -.8*speed.Z;//bounce is not Exactly like its last
	speed.Y = speed.Y;//bounce is not Exactly like its last
	speed.X = .8*speed.X;//bounce is not Exactly like its last
	return true;
      }
      return false;
    }
  };
  ball b;
  struct player {
    int score = 0;
    int gameScore = 0;
    int setScore = 0;
    double size = 0.02;
    int posTEST = 0;
    double relativeX;
    double relativeY;
    vec3 reference;//reference point, bottom left corner for each player 
    /*
      (0, 1)|————(net)————|(1, 1)
      |      |      |
      |      |      |
      |—————————————|(service line)
      |             |
      |      |      |
      |——————p——————|
      (0, 0)        (1, 0)
    */
    vec3 position, direction, speed, maxSpeed, basePosition, serviceBox;
    //how strong this person is, and thus can hit (Max = federer range = 80mph)
    double power = 30;// (max_strength * 1609.35 * 38.284) / (60 * 60 * 60);
    void swing(ball* b, vec3 newDir) {
      b->speed = newDir.times(power);
      b->bounceCount = 0;//resets bounce count
    }
    player(vec3 p, vec3 d, vec3 s, vec3 Ms, vec3 Sb, vec3 ref) :
      position(p),
      direction(d),
      speed(s),
      maxSpeed(Ms),
      basePosition(p),
      serviceBox(Sb),
      reference(ref)
    {}
    bool closeTo(ball *b) {
      double ballDistance = 1;//meters
      if (position.distance(b->position) < ballDistance) {//make z threshold higher, because we can jump
	return true;
      }
      return false;
    }
    bool approached(ball *b) {//returns true or false based off if the ball is coming to you or not
      return std::signbit(b->speed.X) != std::signbit(direction.X);
    }
    double limitSpeed(double current, double MAX) {//takes in a current speed, and limits it to a maximum
      if (current < MAX) {//checks if the current speed is less than the maximum
	return current;//returns the current speed becuase it is less than the max
      }
      else {
	return MAX;//returns the maximum speed, so it cannot be surpassed
      }
    }
    void move(ball *b) {
      if (position.X - reference.X != 0){
	relativeX = direction.X * (position.X - reference.X);//gets the relative position of each player RELATIVE to the left back cornet of their blue section
      }
      else {
	relativeX = 0;//gets the relative position of each player RELATIVE to the left back cornet of their blue section
      }
      relativeY = direction.X * (position.Y - reference.Y);
      
      if (closeTo(b) == false && approached(b) == true) {
	//move to ball
	if (abs(position.Y - b->position.Y) >= 0) {//if the difference between the  Y position of the player and ball is greater than threshold
	  if (position.Y > b->position.Y) {//if greater, then move UP
	    speed.Y = -1 * limitSpeed(abs(position.Y - b->position.Y), maxSpeed.Y);
	  }
	  else if (position.Y < b->position.Y) {//if less than, then move DOWN
	    speed.Y = limitSpeed(abs(position.Y - b->position.Y), maxSpeed.Y);
	  }
	  else {//if not, no need to move
	    speed.Y = 0;//no speed change
	  }
	}
	//X position movement
	if (abs(b->speed.X) < 0.35 * power) {//&& abs(position.X - serviceBox.X) > 0) {//if the ball is going slowly, meaning it will porbably go short, then you'd want to move up to not get a double bounce
	  if (position.X > b->position.X) {//if greater, then move UP
	    speed.X = -1 * limitSpeed(abs(position.X - b->position.X), maxSpeed.X);
	  }
	  if (position.X < b->position.X) {//if greater, then move UP
	    speed.X = limitSpeed(abs(position.X - b->position.X), maxSpeed.X);
	  }
	}
	/*else if (abs(b->speed.X) > 1.1 * power) {//if the ball is going slowly, meaning it will porbably go short, then you'd want to move up to not get a double bounce
	  if (position.X > b->position.X) {//if greater, then move UP
	  speed.X = -1 * limitSpeed(abs(position.X - b->position.X), maxSpeed.X);
	  }
	  if (position.X < b->position.X) {//if greater, then move UP
	  speed.X = limitSpeed(abs(position.X - b->position.X), maxSpeed.X);
	  }
	  }*/
	else {
	  speed.X = 0;//no speed change
	}
      }
      //return to ready position (the center and baseline)
      else {//move to ready position (center) @ 9.1422 meters from the very top of the screen
	if (abs(position.Y - basePosition.Y) >= 0) {//if the difference between the Y position of the player and ball is greater than threshold
	  if (position.Y > basePosition.Y) {//if greater, then move UP
	    speed.Y = -1 * limitSpeed(abs(position.Y - basePosition.Y), maxSpeed.Y);
	  }
	  else if (position.Y < basePosition.Y) {//if less than, then move DOWN
	    speed.Y = limitSpeed(abs(position.Y - basePosition.Y), maxSpeed.Y);
	  }
	  else {//if not, no need to move
	    speed.Y = 0;//no speed change
	  }
	}
	if (abs(position.X - basePosition.X) >= 0) {
	  if (position.X > basePosition.X) {//if greater, then move UP
	    speed.X = -1 * limitSpeed(abs(position.X - basePosition.X), maxSpeed.X);
	  }
	  else if (position.X < basePosition.X) {//if less than, then move DOWN
	    speed.X = limitSpeed(abs(position.X - basePosition.X), maxSpeed.X);
	  }
	  else {//if not, no need to move
	    speed.X = 0;//no speed change
	  }
	}
	//speed.X = 0;//CHANGE LATER
      }
    }
    
    void smartSwing(ball *b, player *p, player *o) {//"intelligently" changes swing direction based off circumstances
      double Xcomp;
      double Ycomp;
      double Zcomp;
      if (closeTo(b) && approached(b)) {//LATER MAKE IT SO THAT IF THE PLAYER IS MOVING AND HITS, THEY WOULD HIT HARDER BECAUSE OF THE MOMENTUM OF PLAYER								   /*CROSSCOURT(80 & )*/
	if (b->waitingForResult && b->bounceCount == 0) {//if first bounce was VOLLEY
	  DATA << "GOOD\n"; DATA.flush();
	  b->waitingForResult = false;
	}
	Ycomp = ((rand() % 225) - 150) / 1000.0;//hit normally(if in center ish)
	/*SHORT(55), MEDIUM(75), DEEP(95)*/
	p->posTEST = 0;
	Xcomp = ((rand() % 20) + 65) / 100.0;//hit normally
	
	if (b->speed.X < 0.7*power) {//if the power is weak (no need to add onto own shot)
	  Xcomp -= (-1 * p->direction.X * 0.1 * (b->speed.X / power));//decrease overall power by a bit
	}
	else if (b->speed.X > 0.9*power) {//if the power is strong (need to add onto own shot)
	  Xcomp += (-1 * p->direction.X * 0.35 * (b->speed.X / power));//increase overall power by a bit ish
	}
	else {
	  Xcomp += 0;
	}
	
	//if (p->position.X )
	Zcomp = Xcomp / 8;
	
	//p->position.X, p->position.Y, p->position.Z, p->direction.X, p->direction.Y, p->direction.Z );
	int precision = 2;//how th numbers should be rounded for the shots to not be so precise
	DATA << playPoint << ", " << "X: " << setprecision(precision) << relativeX
	     << ", " << "Y: " << setprecision(precision) << relativeY << ",            " << setprecision(precision + 1) << /*p->direction.X */ Xcomp
	     << ", " << setprecision(precision + 1) << Ycomp << ", " << setprecision(precision + 1) << Zcomp << ",            "; DATA.flush();
	b->waitingForResult = true;//waiting for if the ball is good or bad
	/*WOULDNT I WANT TOE DIRECTONS TO BE THE SAME FOR BOTH SO I DOONT NEED NEGATIVES*/
	
	relativeX = speed.X * (p->position.X - p->reference.X);
	relativeY = speed.X * (p->position.Y - p->reference.Y);
	swing(b, vec3(p->direction.X * Xcomp, Ycomp, Zcomp));//final "swing" with all directions accounted for.
	//swing(b, vec3(p->direction.X * ((rand() % 40) + 60) / 100.0, ((rand() % 100 - 50) / 1000.0), (((rand() % 30) + 90) / 1000.)));//random power, height, and direction
      }
    }
  };
  player P1;
  player P2;
  
  bool gameType;//singles or doubles /*LATER*/
  void pointEnd(player *p, ball *b) {//takes in which player score changes
    //what happens when the point ends?
    //if (b->waitingForResult) { DATA << "FAIL\n"; DATA.flush(); b->waitingForResult = false;}
    b->speed = vec3(0, 0, 0);
    b->position = vec3(30.17, 9.1422, 1);//resets position to player 1
    P1.position = vec3(30.17, 9.1422, 0);
    P2.position = vec3(6.399, 9.1422, 0);
    if (p->gameScore == 6) { P1.gameScore = 0; p->setScore += 1; P1.score = 0; P2.score = 0; P2.gameScore = 0; }//implement the rule that must win by 2, like 7:5 or tiebreaker
    if (p->score < 30) { p->score += 15; }
    else if (p->score < 40) { p->score += 10; }
    else { P1.score = 0; P2.score = 0; p->gameScore += 1; }//IMPLEMENT DEUCE (40:40)
    //startGame = false;//LINE: 
    startGame = true;//immediately starts the new point
    b->bounceCount = 0;
  }
  void pointResult(bool bounce, ball *b, player *p1, player *p2)//for IF the point would end
  {
    bool net = (b->position.Z < 0.914 && abs(b->position.X - 18.3) < 0.1);
    if (!net && !bounce) {
      // Didn't hit anything.
      return;
    }
    bool good = false;
    //net held at 18.2845 meters
    if (net) {//checking if hit the net
      if (P1.approached(b)) { pointEnd(&P1, b); }//if is coming to P1, then P1 didnt hit it and P1 wins the point
      else { pointEnd(&P2, b); }//if not coming to P1, then P1 did hit it, and P2 wins
    }
    else if (b->bounceCount == 1) {//if the ball has bounced (and FIRST BOUNCE)
      if (b->position.Y < 5.0272 || b->position.Y > 13.2572 || b->position.X > 30.2 || b->position.X < 6.4) {//checking if in doubles alleys(first two), or if within the far back
	if (P1.approached(b)) { pointEnd(&P1, b); }//if coming to P1, therefore P2 hit is, and its any of these, then P1 wins
	else { pointEnd(&P2, b); }//else if coming to P2, P1 hit it, and its out, so P2 wins.
      }
      else if (b->speed.X < 0 && !p1->closeTo(b) && !p2->closeTo(b)) {//if coming from player 1, it hasent already bounced, and you arent hitting it at your feet, (having the ball close to any of the players)
	if (b->position.X > 18.2845) {
	  //IF PLAYER 1's SHOT WAS SHORT, OUTPUT BAD
	  pointEnd(&P2, b);//P2 wins
	}
	else good = true;
      }
      else if (b->speed.X > 0 && !p1->closeTo(b) && !p2->closeTo(b)) {//if coming from player 2
	if (b->position.X < 18.2845) {
	  //IF PLAYER 2's SHOT WAS SHORT, OUTPUT BAD
	  pointEnd(&P1, b);//P1 wins
	}
	else good = true;
      }
      else good = true;
    }
    else if (b->bounceCount == 2) {//checking double bounces
      if (!P1.approached(b)) { pointEnd(&P1, b); }//checking if NOT coming to player 1, then player 1 would win (player 2 got double bounce)
      if (!P2.approached(b)) { pointEnd(&P2, b); }
      return;
    }
    if (b->waitingForResult) {//if first bounce hit the NET
      if (!good){
	DATA << "FAIL\n";
	playPoint++;
      }
      else{
	DATA << "GOOD\n";
	playPoint++;
      }
      DATA.flush();
      b->waitingForResult = false;
    }
  }
  /************************************************************************************************************************************
   ****************************************************************START GAME***********************************************************
   *************************************************************************************************************************************
   *************************************************************************************************************************************/
  game() ://change to degrees?
    
    P1(vec3(30.17, 9.1422, 0),//initial position
       vec3(-1, 0, 0), //initial facing direction
       vec3(0, 0, 0), //initial speed
       vec3(0.1, 0.15, 0), //initial maximum speed
       vec3(24.7, 9.1422, 0),
       vec3(30.17, 3.6572 + 10.973, 0)),//reference point for player 1 (right side)
    P2(vec3(6.399, 9.1422, 0),//initial position
       vec3(1, 0, 0), //initial facing direction
       vec3(0, 0, 0), //initial speed
       vec3(0.1, 0.15, 0), // initial maximum speed
       vec3(11.9, 9.1422, 0),
       vec3(6.399, 3.6572, 0)),// reference point for player 2 (left side).
    b(vec3(30.17, 9.1422, 1),//make sum of direction vector to be 1, normal vector, so power is represented accurately. 
      vec3(0, 0, 0),
      vec3(0, 0, 0), 
      vec3(30.17, 3.6572 + 10.973, 0)) {
  }
  void start() {
    b.acceleration = gravity;
    //P1.swing(&b, vec3(-.7, 0, .1));//initial add
    startGame = true;
    b.bounceCount = 0;
  }
  void advance(double dt) {//basically the update tab
    bool bounce = b.advance(dt);
    pointResult(bounce, &b, &P1, &P2);
    P1.smartSwing(&b, &P1, &P2);//pointer to ball, P1 is hitting, P2 is opponent
    P2.smartSwing(&b, &P2, &P1);//pointer to ball, P2 is hitting, P1 is opponent
    P1.move(&b);
    P2.move(&b);
    
    
    /*if (b.position.Y > 23.5 || b.position.Y < -5.22 || b.position.X > 41.793 || b.position.X < -5.22) {//if far out off the window view, just stop the point
      b.speed = vec3(0, 0, 0);
      b.position = vec3(30.17, 9.1422, 1);//resets position to player 1
      P1.position = vec3(30.17, 9.1422, 0);
      P2.position = vec3(6.399, 9.1422, 0);
      startGame = false;
      }*/
    P1.position = P1.position + P1.speed;//always increment the position by the speed
    P2.position = P2.position + P2.speed;//always increment the position by the speed
    
  }
};

class AutenApp : public AppNative {
public:
  void setup();
  void mouseDown(MouseEvent event);
  void play();
  void update();
  void draw();
  
  game g;
};

int sign(double value) {//returns whether a number is negative or positive.
  if (value<0) { return -1; }
  else { return 1; }
}

/**********************************************************
useful things to note:
- surface & percentage of speed after bouncing:
clay: 59%
acrylic: 68%
grass: 70%

80mi/h = 35.8 m/s = 129 km/h = 7040 ft/min = 117 ft/s = 2347 yds/min
therefore 1mps = 0.447 meters/s
Vf=Vi+(accel)*(time) //time will be constant: 0.97s
- 35.8m/s = 0+(a)(0.97s)
- (35.8m/s)/0.97s = 36.91m/s^2

F=mA therefore: mass of standard tennis ball: 0.056Kg
acceleration of tennis ball: 36.91m/s^2
f = 0.056Kg*36.91m/s^2 = 2.067N
h = -4.9*t^2+V0*t solving for height, V0 is initial velocity

//pixels from : 0.175 width, to 0.825 width
//complete width = 1400px
//1400 * 0.825 - 0.175 * 1400 = 910pixels in 23.77 meters
//therefore: amount of pixels in 1 meter ARE 910/23.77 = 38.284 pixels in one meter.
//  pix    // 40mi     1hr      1     1609.35m     38.28px
// -----  // ------ * ----- * ----- * --------- * --------- ====== 11.41pix/(s/60)
// sec/60//   1hr     3600s    60       1mi          1m

//y=Vy0*t-16*t^2
***********************************************************/
void AutenApp::setup(){
  srand(time(NULL));
  gl::enableVerticalSync();
  setWindowSize(1400, 700);
  
  
}

void AutenApp::mouseDown(MouseEvent event){
  if (event.isLeft() == true) {
    g.start();
  }
}

#if 0
void AutenApp::play() {//what the player does to continue playing
  if (g.turn == Player1) {
    if (abs(b.X - P1.X) < 20 + P1.size && abs(b.Y - P1.Y) < 20 + P1.size) {
      int directionChance = rand() % 100;//based off percentage for ball placement
      int direction;//either 1(R), 2(M), or 3(L), for desired location of the ball 
      if (directionChance >= 0 && directionChance <= P1.percentHitRight) { direction = 1; }//if within first section
      else if (directionChance > P1.percentHitRight && directionChance < P1.percentHitRight + P1.percentHitDownLine) { direction = 2; }//if within second section
      else if (directionChance >= (P1.percentHitDownLine + P1.percentHitRight) && directionChance <= 100) { direction = 3; }//if within third section
      /*
	explanation: (to not have to sort, yet still maintains percentage ratios)
	lets say percents are 35, 20, & 45
	for within first {35%} (0 --> 35)
	for within middle {20%} (35 --> 35 + 20)
	for within last {45%} (35 + 20 ) --> total (all combined, usually 100))
      */
      int accuracy = rand() % 100;
      swing(Player1, direction, accuracy);
      b.turn = !b.turn;//turn goes to the OTHER PERSON
    }
  }
  else if (b.turn == Player2) {
    if (abs(b.X - P2.X) < 20 + P2.size && abs(b.Y - P2.Y) < 20 + P2.size) {
      int directionChance = rand() % 100;//based off percentage for ball placement
      int direction;//either 1(R), 2(M), or 3(L), for desired location of the ball 
      if (directionChance >= 0 && directionChance <= P2.percentHitRight) { direction = 1; }//if within first section
      else if (directionChance > P2.percentHitRight && directionChance < P2.percentHitRight + P2.percentHitDownLine) { direction = 2; }//if within second section
      else if (directionChance >= (P2.percentHitDownLine + P2.percentHitRight) && directionChance <= 100) { direction = 3; }//if within third section
      /*
	explanation: (to not have to sort, yet still maintains percentage ratios)
	lets say percents are 35, 20, & 45
	for within first {35%} (0 --> 35)
	for within middle {20%} (35 --> 35 + 20)
	for within last {45%} (35 + 20 ) --> total (all combined, usually 100))
      */
      int accuracy = rand() % 100;
      swing(Player2, direction, accuracy);
      b.turn = !b.turn;//turn goes to the OTHER PERSON
    }
  }
}
#endif

void AutenApp::update()
{
  double dt = 1.0 / 60.0;  // 60 hz refresh
  
#if 0
  //START RULES
  if (b.Z <= 0) {//for ball BOUNCES
    b.number_bounces++;
    if (b.number_bounces == 2) {
      //if two bounces on player 1's side, then the opponent gets the point
      if (b.turn == Player1) { P2.score++; P1.feeling--; P2.feeling++; }
      //else if they get two bounces, player 1 gets the point
      else { P1.score++; P2.feeling--; P1.feeling++; }
    }
    if (b.overNet == true) {//meaning, they must have continued the point, with hitting the ball over
      b.number_bounces = 0;//resets number of bounces, because not counting for the OTHER person
    }
    //if (gameType == doubles)
    //if (b.X < .175*getWindowWidth() - 5 || b.X < 0.2 * getWindowHeight() - 5 || b.X > 0.825 * getWindowWidth() + 5 || b.X > 0.8 * getWindowHeight() + 5) {//if the X of the ball is OUT
    if (gameType == singles) {
      if (b.X < .175 * getWindowWidth() - 5 || b.Y < 0.275 * getWindowHeight() - 5 || b.X > 0.825 * getWindowWidth() + 5 || b.Y > 0.7283 * getWindowHeight() + 5) {//if the X of the ball is OUT
	if (b.lastHit == Player1) { P2.score++; P1.feeling--; P2.feeling++; }//if the last person who hit the ball, hit it OUT in any way... other person gets point
	else { P1.score++; P2.feeling--; P1.feeling++; }//if the last person who hit the ball, hit it OUT in any way... other person gets point
      }
    }
    //b.angleHit = asin(P1.desiredHeight / (P1.max_strength / 2))* (180 / PI);
    TIME = 0;//resets time counter
    b.initVel = b.initVel * 0.78;//(P1.max_strength * 1609.39) / (3600);
  }
#endif
  //FINISHED RULES
  //START GAMEPLAY
  if (g.startGame == true) { g.advance(dt); }
#if 0
  if (start == true) {
    if (P1.feeling <= 0) {//if in ANY WAY upset, wants to WINNNNNN (plays more competitively)
      win();
    }
    if (P1.feeling > 0) {//if in ANY way happy, just wants to continue playing. 
      play();
    }
    if (P2.feeling <= 0) {//if in ANY WAY upset, wants to WINNNNNN (plays more competitively)
      win();
    }
    if (P2.feeling > 0) {//if in ANY way happy, just wants to continue playing. 
      play();
    }
    
    if (b.lastHit == Player1) {
      b.X = b.X - (b.acceleration);//
    }
    else {
      b.X = b.X + (b.acceleration);//
    }
    if (b.acceleration > 0) { b.acceleration -= 0.25; }//change ball acceleration    /*change from weird constant... when get chance of course*/
    b.Z = (b.initVel * realTime) - (16 * ((realTime)*(realTime)));
    //b.Z = -9.8 * (realTime * realTime) + b.initVel * (realTime);//using meters, and milliseconds
    //b.acceleration = sign(b.acceleration)*(abs(b.acceleration - .1));
    /*WHAT NEED: NEED TO SAY WHAT ANGLE THE SHOT WILL BE TAKEN AT, THEN LOOK INTO HOW THE ANGLE WILL AFFECT EH TRAJECTORY OF THE BALL. */
    //timer
    TIME += 1;//refreshes with the refresh rate of the program, 60hz
    realTime = (TIME / 60);//heres the math:
    //       TIME         X(ms)
    //      ------   =   -------
    //      60(hz)        1 (1s)
    
  }
#endif
  
  //other random trash
  //maintain aspect ratio
  //setWindowSize(getWindowWidth(), getWindowWidth() / 2);//scalar of 1 to 2 (MAINTAINING ASPECT RATIO)
  //ct.amountOfPixels = ((0.825 * getWindowWidth()) - (0.175 * getWindowWidth()));
  //ct.pixelsPerMeter = ct.amountOfPixels / 23.77; //gives amount of pixels in a meter. 
  //need them to keep moving whilst on the screen, even if window size changes
  /*P1.X = 0.825 * getWindowWidth();
    P1.Y = 0.5 * getWindowHeight();
    P1.size = getWindowHeight() * 0.03;
    //maintain P2's location
    P2.X = 0.175 * getWindowWidth();
    P2.Y = 0.5 * getWindowHeight();
    P2.size = getWindowHeight() * 0.03;*/
}

void AutenApp::draw()
{
	// clear out the window with black
  gl::clear(Color(0, 0, 0));
  
  gl::color(0, 0, 1);
  gl::drawSolidRect(Rectf(0.175 * getWindowWidth(), 0.2 * getWindowHeight(), 0.825 * getWindowWidth(), 0.8 * getWindowHeight()));
  gl::color(1, 1, 1);
  //aspect ratio of 2:1
  gl::drawStrokedRect(Rectf(0.175 * getWindowWidth(), 0.2 * getWindowHeight(), 0.825 * getWindowWidth(), 0.8 * getWindowHeight()));//draws hollow rectangle (outline of court)
  //now for the lines
  gl::drawStrokedRect(Rectf(.175*getWindowWidth(), 0.275*getWindowHeight(), 0.825 * getWindowWidth(), 0.7283*getWindowHeight()));
  gl::drawStrokedRect(Rectf(.325*getWindowWidth(), 0.275*getWindowHeight(), 0.675 * getWindowWidth(), 0.7283*getWindowHeight()));
  gl::drawSolidRect(Rectf(.325*getWindowWidth(), 0.5*getWindowHeight() - 1, 0.675 * getWindowWidth(), 0.5*getWindowHeight() + 1));//uses +-1 to set line thickness
  gl::drawSolidRect(Rectf(.175*getWindowWidth(), 0.5*getWindowHeight() - 1, 0.2 * getWindowWidth(), 0.5*getWindowHeight() + 1));
  gl::drawSolidRect(Rectf(.8*getWindowWidth(), 0.5*getWindowHeight() - 1, 0.825 * getWindowWidth(), 0.5*getWindowHeight() + 1));
  //draw NET
  gl::drawSolidRect(Rectf(.5*getWindowWidth() - 1, 0.2*getWindowHeight(), 0.5 * getWindowWidth() + 1, 0.8*getWindowHeight()));
  //player 1
  gl::color(0, 1, 0);
  gl::drawSolidRect(Rectf(
			  (g.P1.position.X * pixelsPerMeter) - g.P1.size*getWindowHeight(),
			  (g.P1.position.Y * pixelsPerMeter) - g.P1.size*getWindowHeight(),
			  (g.P1.position.X * pixelsPerMeter) + g.P1.size*getWindowHeight(),
			  (g.P1.position.Y * pixelsPerMeter) + g.P1.size*getWindowHeight()));
  gl::color(1, 1, 1);
  //player 2
	gl::color(1, 0, 0);
	gl::drawSolidRect(Rectf(
				(g.P2.position.X * pixelsPerMeter) - g.P2.size*getWindowHeight(),
				(g.P2.position.Y * pixelsPerMeter) - g.P2.size*getWindowHeight(),
				(g.P2.position.X * pixelsPerMeter) + g.P2.size*getWindowHeight(),
				(g.P2.position.Y * pixelsPerMeter) + g.P2.size*getWindowHeight()));
	gl::color(1, 1, 1);
	//ball
	if (g.b.position.Z > 0.1) {
	  gl::color(255, 255, 0);
	  gl::drawSolidCircle(Vec2f(g.b.position.X* pixelsPerMeter, g.b.position.Y* pixelsPerMeter), 0.01f * getWindowHeight() + 10 * g.b.position.Z);
	}
	else {
	  gl::color(1, 0, 0);
	  gl::drawSolidCircle(Vec2f(g.b.position.X* pixelsPerMeter, g.b.position.Y* pixelsPerMeter), 0.01f * getWindowHeight() + 10 * g.b.position.Z);
	  
	}
	
	gl::color(1, 0, 0);
	gl::drawSolidRect(Rectf(g.P2.reference.X*pixelsPerMeter, g.P2.reference.Y*pixelsPerMeter, g.P2.reference.X*pixelsPerMeter + 50, g.P2.reference.Y*pixelsPerMeter + 50));
	gl::color(1, 1, 1);
	//outline demonstrating ball bounces
	if (g.b.position.Z <= 0.1) {
	  double cX = g.b.position.X * pixelsPerMeter, cY = g.b.position.Y * pixelsPerMeter;
	  gl::color(255, 0, 0);
	  gl::drawStrokedCircle(Vec2f(cX, cY), 0.03f * getWindowHeight());
	  gl::drawStrokedCircle(Vec2f(cX, cY), 0.05f * getWindowHeight());
	  gl::drawStrokedCircle(Vec2f(cX, cY), 0.07f * getWindowHeight());
	  gl::color(1, 1, 1);
	}
	
	
	stringstream P1score;
	string P1score2;
	P1score << g.P1.score;
	P1score >> P1score2;
	gl::drawString(P1score2, Vec2f(1200, 100), Color(1, 1, 1), Font("Arial", 70));
	
	stringstream P2score;
	string P2score2;
	P2score << g.P2.score;
	P2score >> P2score2;
	gl::drawString(P2score2, Vec2f(100, 100), Color(1, 1, 1), Font("Arial", 70));
	
	stringstream P1gamescore;
	string P1gamescore2;
	P1gamescore << g.P1.gameScore;
	P1gamescore >> P1gamescore2;
	gl::drawString("Game", Vec2f(1050, 20), Color(1, 1, 1), Font("Arial", 40));
	gl::drawString(P1gamescore2, Vec2f(1050, 80), Color(1, 1, 1), Font("Arial", 60));

	stringstream P2gamescore;
	string P2gamescore2;
	P2gamescore << g.P2.gameScore;
	P2gamescore >> P2gamescore2;
	gl::drawString("Game", Vec2f(350, 20), Color(1, 1, 1), Font("Arial", 40));
	gl::drawString(P2gamescore2, Vec2f(350, 80), Color(1, 1, 1), Font("Arial", 60));

	stringstream P2setscore;
	string P2setscore2;
	P2setscore << g.P2.setScore;
	P2setscore >> P2setscore2;
	gl::drawString("Set", Vec2f(700, 60), Color(1, 1, 1), Font("Arial", 30));
	gl::drawString(P2setscore2, Vec2f(600, 80), Color(1, 1, 1), Font("Arial", 50));

	stringstream P1setscore;
	string P1setscore2;
	P1setscore << g.P1.setScore;
	P1setscore >> P1setscore2;
	gl::drawString(P1setscore2, Vec2f(800, 80), Color(1, 1, 1), Font("Arial", 50));

	//debug
	stringstream bHeight;
	string bHeight2;
	bHeight << g.b.position.Z;
	bHeight >> bHeight2;
	gl::drawString(bHeight2, Vec2f(1200, 400), Color(1, 1, 1), Font("Arial", 30));

	stringstream bpower;
	string bpower2;
	bpower << g.b.speed.X;
	bpower >> bpower2;
	gl::drawString(bpower2, Vec2f(1000, 500), Color(1, 1, 1), Font("Arial", 30));

	stringstream bounces;
	string bounces2;
	bounces << g.b.bounceCount;
	bounces >> bounces2;
	gl::drawString("Bounces:", Vec2f(1200, 500), Color(1, 1, 1), Font("Arial", 30));
	gl::drawString(bounces2, Vec2f(1280, 500), Color(1, 1, 1), Font("Arial", 30));
	/*
	  stringstream posTest1;
	string posTest2;
	posTest1 << play;
	posTest1 >> posTest2;
	//gl::drawString("Bounces:", Vec2f(30, 500), Color(1, 1, 1), Font("Arial", 30));
	gl::drawString(posTest2, Vec2f(50, 500), Color(1, 1, 1), Font("Arial", 30));
	*/
	
	string stringRead;
	if (playPoint == 6){
	  //stringRead = getline("DATA.txt", playPoint);
	}
	gl::drawString(stringRead, Vec2f(500, 600), Color(1, 1, 1), Font("Arial", 30));
	

	
	stringstream posY1;
	string posY2;
	posY1 << g.b.relativeY;
	posY1 >> posY2;
	gl::drawString("relY:", Vec2f(20, 500), Color(1, 1, 1), Font("Arial", 30));
	gl::drawString(posY2, Vec2f(60, 500), Color(1, 1, 1), Font("Arial", 30));
	
	stringstream pos1;
	string pos2;
	pos1 << g.b.relativeX;
	pos1 >> pos2;
	gl::drawString("relX:", Vec2f(20, 630), Color(1, 1, 1), Font("Arial", 30));
	gl::drawString(pos2, Vec2f(60, 630), Color(1, 1, 1), Font("Arial", 30));
}

CINDER_APP_NATIVE(AutenApp, RendererGl)
