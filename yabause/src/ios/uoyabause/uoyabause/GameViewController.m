//
//  GameViewController.m
//  uoyabause
//
//  Created by MiyamotoShinya on 2016/02/06.
//  Copyright © 2016年 devMiyax. All rights reserved.
//

#import "GameViewController.h"
#import <OpenGLES/ES2/glext.h>
@import GameController;

#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AudioToolbox/ExtendedAudioFile.h>

/** @defgroup pad Pad
 *
 * @{
 */


#define PERPAD_UP	0
#define PERPAD_RIGHT	1
#define PERPAD_DOWN	2
#define PERPAD_LEFT	3
#define PERPAD_RIGHT_TRIGGER 4
#define PERPAD_LEFT_TRIGGER 5
#define PERPAD_START	6
#define PERPAD_A	7
#define PERPAD_B	8
#define PERPAD_C	9
#define PERPAD_X	10
#define PERPAD_Y	11
#define PERPAD_Z	12

#define CART_NONE            0
#define CART_PAR             1
#define CART_BACKUPRAM4MBIT  2
#define CART_BACKUPRAM8MBIT  3
#define CART_BACKUPRAM16MBIT 4
#define CART_BACKUPRAM32MBIT 5
#define CART_DRAM8MBIT       6
#define CART_DRAM32MBIT      7
#define CART_NETLINK         8
#define CART_ROM16MBIT       9

void PerKeyDown(unsigned int key);
void PerKeyUp(unsigned int key);
int start_emulation( int width, int height );
int emulation_step();
int enterBackGround();

EAGLContext *g_context = nil;
EAGLContext *g_share_context = nil;

// Settings
BOOL _bios =YES;
int _cart = 0;
BOOL _fps = NO;
BOOL _frame_skip = NO;
BOOL _aspect_rate = NO;
int _filter = 0;
int _sound_engine = 0;


@interface GameViewController () {
    GLuint _program;
    
    GLKMatrix4 _modelViewProjectionMatrix;
    GLKMatrix3 _normalMatrix;
    float _rotation;
    
    GLuint _vertexArray;
    GLuint _vertexBuffer;
    
  
}
@property (strong, nonatomic) EAGLContext *context;
@property (strong, nonatomic) EAGLContext *share_context;
@property (nonatomic, strong) GCController *controller;

- (void)setupGL;
- (void)tearDownGL;

@end

@implementation GameViewController
@synthesize iPodIsPlaying;
static GameViewController *sharedData_ = nil;

// C "trampoline" function to invoke Objective-C method
int swapAglBuffer ()
{
    EAGLContext* context = [EAGLContext currentContext];
    [context presentRenderbuffer:GL_RENDERBUFFER];
    return 0;
}

- (void)loadSettings {

    NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
    _bios = [userDefaults boolForKey: @"bios"];
    _cart = (int)[userDefaults integerForKey: @"cart"];
    _fps = [userDefaults boolForKey: @"fps"];
    _frame_skip = [userDefaults boolForKey: @"frame_skip"];
    _aspect_rate = [userDefaults boolForKey: @"aspect_rate"];
    _filter = [userDefaults boolForKey: @"aspect_rate"];
    _sound_engine = [userDefaults boolForKey: @"sound_engine"];
}

void RevokeOGLOnThisThread(){
    [EAGLContext setCurrentContext:g_share_context];
}

void UseOGLOnThisThread(){
    [EAGLContext setCurrentContext:g_context];
}

const char * GetBiosPath(){
    if( _bios == YES ){
        return NULL;
    }
    NSString * path = [[NSBundle mainBundle] pathForResource:  @"bios" ofType: @"bin"];
    return [path cStringUsingEncoding:1];
    //return NULL;
}

const char * GetGamePath(){
    
    if( sharedData_ == nil ){
        return nil;
    }
    NSString *path = sharedData_.selected_file;
    return [path cStringUsingEncoding:1];
}

const char * GetMemoryPath(){
    BOOL isDir;
    NSFileManager *filemgr;
    filemgr = [NSFileManager defaultManager];
    NSString * fileName = @"backup/memory.bin";
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    
    NSString *filePath = [documentsDirectory stringByAppendingPathComponent: fileName];
    NSLog(@"full path name: %@", filePath);
    
    
    NSString *docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
    NSString *dirName = [docDir stringByAppendingPathComponent:@"backup"];
    
    
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:dirName isDirectory:&isDir])
    {
        if([fm createDirectoryAtPath:dirName withIntermediateDirectories:YES attributes:nil error:nil])
            NSLog(@"Directory Created");
        else
            NSLog(@"Directory Creation Failed");
    }
    else
        NSLog(@"Directory Already Exist");
    
    // check if file exists
    if ([filemgr fileExistsAtPath: filePath] == YES){
        NSLog(@"File exists");
        
    }else {
        NSLog (@"File not found, file will be created");
    }
    
    return [filePath fileSystemRepresentation];
}

int GetCartridgeType(){
    return _cart;
}

int GetVideoInterface(){
    return 0;
}


const char * GetCartridgePath(){
    BOOL isDir;
    NSFileManager *filemgr;
    filemgr = [NSFileManager defaultManager];
    NSString * fileName = @"cart/invalid.ram";
    
    switch(_cart) {
        case CART_NONE:
            fileName = @"cart/none.ram";
        case CART_PAR:
            fileName = @"cart/par.ram";
        case CART_BACKUPRAM4MBIT:
            fileName = @"cart/backup4.ram";
        case CART_BACKUPRAM8MBIT:
            fileName = @"cart/backup8.ram";
        case CART_BACKUPRAM16MBIT:
            fileName = @"cart/backup16.ram";
        case CART_BACKUPRAM32MBIT:
            fileName = @"cart/backup32.ram";
        case CART_DRAM8MBIT:
            fileName = @"cart/dram8.ram";
        case CART_DRAM32MBIT:
            fileName = @"cart/dram32.ram";
        case CART_NETLINK:
            fileName = @"cart/netlink.ram";
        case CART_ROM16MBIT:
            fileName = @"cart/om16.ram";
        default:
            fileName = @"cart/invalid.ram";
    }
    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    
    NSString *filePath = [documentsDirectory stringByAppendingPathComponent: fileName];
    NSLog(@"full path name: %@", filePath);
    
    
    NSString *docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
    NSString *dirName = [docDir stringByAppendingPathComponent:@"cart"];
    
    
    NSFileManager *fm = [NSFileManager defaultManager];
    if(![fm fileExistsAtPath:dirName isDirectory:&isDir])
    {
        if([fm createDirectoryAtPath:dirName withIntermediateDirectories:YES attributes:nil error:nil])
            NSLog(@"Directory Created");
        else
            NSLog(@"Directory Creation Failed");
    }
    else
        NSLog(@"Directory Already Exist");
    
    // check if file exists
    if ([filemgr fileExistsAtPath: filePath] == YES){
        NSLog(@"File exists");
        
    }else {
        NSLog (@"File not found, file will be created");
    }
    return [filePath fileSystemRepresentation];
}

int GetPlayer2Device(){
    return -1;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{

    if( [self hasControllerConnected ] ) return;
    
    int i=0;
    NSSet *allTouches = [event allTouches];
    for (UITouch *touch in allTouches)
    {
        CGPoint point = [touch locationInView:[self view]];
        
        if( CGRectContainsPoint([ [self right_button ]frame ], point) ){
            [self right_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_RIGHT);
        }
        if( CGRectContainsPoint([ [self left_button ]frame ], point) ){
            [self left_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_LEFT);
        }
        if( CGRectContainsPoint([ [self up_button ]frame ], point) ){
            [self up_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_UP);
        }
        if( CGRectContainsPoint([ [self down_button ]frame ], point) ){
            [self down_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_DOWN);
        }
        if( CGRectContainsPoint([ [self left_trigger ]frame ], point) ){
            [self left_trigger ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_LEFT_TRIGGER);
        }
        if( CGRectContainsPoint([ [self right_trigger ]frame ], point) ){
            [self right_trigger ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_RIGHT_TRIGGER);
        }
        if( CGRectContainsPoint([ [self a_button ]frame ], point) ){
            [self a_button ].backgroundColor = [UIColor redColor];
             PerKeyDown(PERPAD_A);
        }
        if( CGRectContainsPoint([ [self b_button ]frame ], point) ){
            [self b_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_B);
        }
        if( CGRectContainsPoint([ [self c_button ]frame ], point) ){
            [self c_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_C);
        }
        if( CGRectContainsPoint([ [self x_button ]frame ], point) ){
            [self x_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_X);
        }
        if( CGRectContainsPoint([ [self y_button ]frame ], point) ){
            [self y_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_Y);
        }
        if( CGRectContainsPoint([ [self z_button ]frame ], point) ){
            [self z_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_Z);
        }
        if( CGRectContainsPoint([ [self start_button ]frame ], point) ){
            [self start_button ].backgroundColor = [UIColor redColor];
            PerKeyDown(PERPAD_START);
        }
        
        i++;
    }
    


}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    if( [self hasControllerConnected ] ) return;
    
    int i=0;
    NSSet *allTouches = [event allTouches];
    for (UITouch *touch in allTouches)
    {
        CGPoint point = [touch locationInView:[self view]];
        
        if( CGRectContainsPoint([ [self left_panel ]frame ], point)){
        
            if( CGRectContainsPoint([ [self right_button ]frame ], point) ){
                [self right_button ].backgroundColor = [UIColor redColor];
                PerKeyDown(PERPAD_RIGHT);
            }else{
                [self right_button ].backgroundColor = [UIColor darkGrayColor];
                PerKeyUp(PERPAD_RIGHT);
            }
            if( CGRectContainsPoint([ [self left_button ]frame ], point) ){
                [self left_button ].backgroundColor = [UIColor redColor];
                PerKeyDown(PERPAD_LEFT);
            }else{
                [self left_button ].backgroundColor = [UIColor darkGrayColor];
                PerKeyUp(PERPAD_LEFT);
            }
            if( CGRectContainsPoint([ [self up_button ]frame ], point) ){
                [self up_button ].backgroundColor = [UIColor redColor];
                PerKeyDown(PERPAD_UP);
            }else{
                [self up_button ].backgroundColor = [UIColor darkGrayColor];
                PerKeyUp(PERPAD_UP);
            }
            if( CGRectContainsPoint([ [self down_button ]frame ], point) ){
                [self down_button ].backgroundColor = [UIColor redColor];
                PerKeyDown(PERPAD_DOWN);
            }else{
                [self down_button ].backgroundColor = [UIColor darkGrayColor];
                PerKeyUp(PERPAD_DOWN);
            }
        }

        i++;
    }

}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    if( [self hasControllerConnected ] ) return;
    
    int i=0;
    NSSet *allTouches = [event allTouches];
    for (UITouch *touch in allTouches)
    {
        CGPoint point = [touch locationInView:[self view]];
        if( CGRectContainsPoint([ [self right_button ]frame ], point) ){
            [self right_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_RIGHT);
        }
        if( CGRectContainsPoint([ [self left_button ]frame ], point) ){
            [self left_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_LEFT);
        }
        if( CGRectContainsPoint([ [self up_button ]frame ], point) ){
            [self up_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_UP);
        }
        if( CGRectContainsPoint([ [self down_button ]frame ], point) ){
            [self down_button ].backgroundColor = [UIColor darkGrayColor];
             PerKeyUp(PERPAD_DOWN);
        }
        if( CGRectContainsPoint([ [self left_trigger ]frame ], point) ){
            [self left_trigger ].backgroundColor = [UIColor darkGrayColor];
              PerKeyUp(PERPAD_LEFT_TRIGGER);
        }
        if( CGRectContainsPoint([ [self right_trigger ]frame ], point) ){
             [self right_trigger ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_RIGHT_TRIGGER);
        }
        if( CGRectContainsPoint([ [self a_button ]frame ], point) ){
            [self a_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_A);
        }
        if( CGRectContainsPoint([ [self b_button ]frame ], point) ){
            [self b_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_B);
        }
        if( CGRectContainsPoint([ [self c_button ]frame ], point) ){
            [self c_button ].backgroundColor = [UIColor darkGrayColor];
             PerKeyUp(PERPAD_C);
        }
        if( CGRectContainsPoint([ [self x_button ]frame ], point) ){
            [self x_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_X);
        }
        if( CGRectContainsPoint([ [self y_button ]frame ], point) ){
            [self y_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_Y);
        }
        if( CGRectContainsPoint([ [self z_button ]frame ], point) ){
            [self z_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_Z);
        }
        if( CGRectContainsPoint([ [self start_button ]frame ], point) ){
            [self start_button ].backgroundColor = [UIColor darkGrayColor];
            PerKeyUp(PERPAD_START);
        }
        i++;
    }
}

#pragma mark AVAudioSession
- (void)handleInterruption:(NSNotification *)notification
{
    UInt8 theInterruptionType = [[notification.userInfo valueForKey:AVAudioSessionInterruptionTypeKey] intValue];
    
    NSLog(@"Session interrupted > --- %s ---\n", theInterruptionType == AVAudioSessionInterruptionTypeBegan ? "Begin Interruption" : "End Interruption");
    
    if (theInterruptionType == AVAudioSessionInterruptionTypeBegan) {
        //alcMakeContextCurrent(NULL);
        //if (self.isPlaying) {
        //    self.wasInterrupted = YES;
        //}
    } else if (theInterruptionType == AVAudioSessionInterruptionTypeEnded) {
        // make sure to activate the session
        NSError *error;
        bool success = [[AVAudioSession sharedInstance] setActive:YES error:&error];
        if (!success) NSLog(@"Error setting session active! %@\n", [error localizedDescription]);
        
        //alcMakeContextCurrent(self.context);
        
        //if (self.wasInterrupted)
        //{
         //   [self startSound];
         //   self.wasInterrupted = NO;
        //}
    }
}

#pragma mark -Audio Session Route Change Notification

- (void)handleRouteChange:(NSNotification *)notification
{
    UInt8 reasonValue = [[notification.userInfo valueForKey:AVAudioSessionRouteChangeReasonKey] intValue];
    AVAudioSessionRouteDescription *routeDescription = [notification.userInfo valueForKey:AVAudioSessionRouteChangePreviousRouteKey];
    
    NSLog(@"Route change:");
    switch (reasonValue) {
        case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
            NSLog(@"     NewDeviceAvailable");
            break;
        case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
            NSLog(@"     OldDeviceUnavailable");
            break;
        case AVAudioSessionRouteChangeReasonCategoryChange:
            NSLog(@"     CategoryChange");
            NSLog(@" New Category: %@", [[AVAudioSession sharedInstance] category]);
            break;
        case AVAudioSessionRouteChangeReasonOverride:
            NSLog(@"     Override");
            break;
        case AVAudioSessionRouteChangeReasonWakeFromSleep:
            NSLog(@"     WakeFromSleep");
            break;
        case AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory:
            NSLog(@"     NoSuitableRouteForCategory");
            break;
        default:
            NSLog(@"     ReasonUnknown");
    }
    
    NSLog(@"Previous route:\n");
    NSLog(@"%@", routeDescription);
}

- (void)controllerDidConnect:(NSNotification *)notification
{
    
}

- (void)controllerDidDisconnect
{
    [self left_panel ].hidden = NO;
    [self right_panel ].hidden = NO;
    [self left_button ].hidden = NO;
    [self right_button ].hidden = NO;
    [self up_button ].hidden = NO;
    [self down_button ].hidden = NO;
    [self a_button ].hidden = NO;
    [self b_button ].hidden = NO;
    [self c_button ].hidden = NO;
    [self x_button ].hidden = NO;
    [self y_button ].hidden = NO;
    [self z_button ].hidden = NO;
    [self left_trigger ].hidden = NO;
    [self right_trigger ].hidden = NO;
    [self start_button ].hidden = NO;
}

-(void)completionWirelessControllerDiscovery
{
    if( [GCController controllers].count >= 1 ){
        self.controller = [GCController controllers][0];
        if (self.controller.gamepad) {
            
            [self.controller.gamepad.buttonA setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_A);
                }else{
                    PerKeyUp(PERPAD_A);
                }
            }];
            
            [self.controller.extendedGamepad.rightShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_C);
                }else{
                    PerKeyUp(PERPAD_C);
                }
            }];
            
            [self.controller.extendedGamepad.leftShoulder setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_Z);
                }else{
                    PerKeyUp(PERPAD_Z);
                }
            }];
            
            [self.controller.extendedGamepad.leftTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_LEFT_TRIGGER);
                }else{
                    PerKeyUp(PERPAD_LEFT_TRIGGER);
                }
            }];
            
            [self.controller.extendedGamepad.rightTrigger setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_RIGHT_TRIGGER);
                }else{
                    PerKeyUp(PERPAD_RIGHT_TRIGGER);
                }
            }];
            
            [self.controller.gamepad.buttonX setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_X);
                }else{
                    PerKeyUp(PERPAD_X);
                }
            }];
            [self.controller.gamepad.buttonY setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_Y);
                }else{
                    PerKeyUp(PERPAD_Y);
                }
            }];
            [self.controller.gamepad.buttonB setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_B);
                }else{
                    PerKeyUp(PERPAD_B);
                }
            }];
            [self.controller.gamepad.dpad.up setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_UP);
                }else{
                    PerKeyUp(PERPAD_UP);
                }
            }];
            [self.controller.gamepad.dpad.down setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_DOWN);
                }else{
                    PerKeyUp(PERPAD_DOWN);
                }
            }];
            [self.controller.gamepad.dpad.left setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_LEFT);
                }else{
                    PerKeyUp(PERPAD_LEFT);
                }
            }];
            [self.controller.gamepad.dpad.right setValueChangedHandler:^(GCControllerButtonInput *button, float value, BOOL pressed) {
                if(pressed){
                    PerKeyDown(PERPAD_RIGHT);
                }else{
                    PerKeyUp(PERPAD_RIGHT);
                }
            }];
            [self.controller.extendedGamepad.rightThumbstick setValueChangedHandler:^(GCControllerDirectionPad *dpad, float xValue, float yValue) {
                if(yValue >= 0.5 || yValue <= -0.5 ){
                    PerKeyDown(PERPAD_START);
                }else{
                    PerKeyUp(PERPAD_START);
                }
            }];
        }
    }
    
}

- (BOOL)hasControllerConnected {
    return [[GCController controllers] count] > 0;
}


- (void)viewDidAppear:(BOOL)animated
{
    
    if ([self hasControllerConnected]) {
        NSLog(@"Discovery finished on first pass");
        [self foundController];
    } else {
        NSLog(@"Discovery happening patiently");
        [self patientlyDiscoverController];
    }
}

- (void)patientlyDiscoverController {
    
    [GCController startWirelessControllerDiscoveryWithCompletionHandler:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(foundController)
                                                 name:GCControllerDidConnectNotification
                                               object:nil];
}   

- (void)foundController {
    NSLog(@"Found Controller");
    

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(controllerDidDisconnect)
                                                 name:GCControllerDidDisconnectNotification
                                               object:nil];
    
    [self left_panel ].hidden = YES;
    [self right_panel ].hidden = YES;
    [self left_button ].hidden = YES;
    [self right_button ].hidden = YES;
    [self up_button ].hidden = YES;
    [self down_button ].hidden = YES;
    [self a_button ].hidden = YES;
    [self b_button ].hidden = YES;
    [self c_button ].hidden = YES;
    [self x_button ].hidden = YES;
    [self y_button ].hidden = YES;
    [self z_button ].hidden = YES;
    [self left_trigger ].hidden = YES;
    [self right_trigger ].hidden = YES;
    [self start_button ].hidden = YES;
    
    [self completionWirelessControllerDiscovery];
}


- (void)viewDidLoad
{
    sharedData_ = self;
    [super viewDidLoad];
    
    self.view.multipleTouchEnabled = YES;
    

    [self left_button ].alpha = 0.0f;
    [self right_button ].alpha = 0.0f;
    [self up_button ].alpha = 0.0f;
    [self down_button ].alpha = 0.0f;
    [self a_button ].alpha = 0.0f;
    [self b_button ].alpha = 0.0f;
    [self c_button ].alpha = 0.0f;
    [self x_button ].alpha = 0.0f;
    [self y_button ].alpha = 0.0f;
    [self z_button ].alpha = 0.0f;
    [self left_trigger ].alpha = 0.0f;
    [self right_trigger ].alpha = 0.0f;
    [self start_button ].alpha = 0.0f;
    
    [self loadSettings];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didEnterBackground) name:UIApplicationDidEnterBackgroundNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didBecomeActive) name:UIApplicationDidBecomeActiveNotification object:nil];
    
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    //self.context.multiThreaded = YES;
    self.share_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3 sharegroup:[self.context sharegroup] ];

    if (!self.context) {
        NSLog(@"Failed to create ES context");
    }

    g_share_context = self.share_context;
    g_context = self.context;
    
    GLKView *view = (GLKView *)self.view;
    view.context = self.context;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
    view.drawableStencilFormat = GLKViewDrawableStencilFormat8;

    // Configure the audio session
    AVAudioSession *sessionInstance = [AVAudioSession sharedInstance];
    NSError *error;
    
    // set the session category
    iPodIsPlaying = [sessionInstance isOtherAudioPlaying];
    NSString *category = iPodIsPlaying ? AVAudioSessionCategoryAmbient : AVAudioSessionCategorySoloAmbient;
    bool success = [sessionInstance setCategory:category error:&error];
    if (!success) NSLog(@"Error setting AVAudioSession category! %@\n", [error localizedDescription]);
    
    double hwSampleRate = 44100.0;
    success = [sessionInstance setPreferredSampleRate:hwSampleRate error:&error];
    if (!success) NSLog(@"Error setting preferred sample rate! %@\n", [error localizedDescription]);
    
    // add interruption handler
    [[NSNotificationCenter defaultCenter]   addObserver:self
                                               selector:@selector(handleInterruption:)
                                                   name:AVAudioSessionInterruptionNotification
                                                 object:sessionInstance];
    
    // we don't do anything special in the route change notification
    [[NSNotificationCenter defaultCenter]   addObserver:self
                                               selector:@selector(handleRouteChange:)
                                                   name:AVAudioSessionRouteChangeNotification
                                                 object:sessionInstance];
    
    // activate the audio session
    success = [sessionInstance setActive:YES error:&error];
    if (!success) NSLog(@"Error setting session active! %@\n", [error localizedDescription]);

   
    
    self.preferredFramesPerSecond =120;

    
  
    [self setupGL];
}

- (void)dealloc
{    
    [self tearDownGL];
    
    if ([EAGLContext currentContext] == self.context) {
        [EAGLContext setCurrentContext:nil];
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];

    if ([self isViewLoaded] && ([[self view] window] == nil)) {
        self.view = nil;
        
        [self tearDownGL];
        
        if ([EAGLContext currentContext] == self.context) {
            [EAGLContext setCurrentContext:nil];
        }
        self.context = nil;
    }

    // Dispose of any resources that can be recreated.
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (void)setupGL
{
    [EAGLContext setCurrentContext:self.context];
    GLKView *view = (GLKView *)self.view;
    view.context = self.context;
     [view bindDrawable ];
    
    start_emulation(1920,1080);
    
}

- (void)tearDownGL
{
    [EAGLContext setCurrentContext:self.context];
   
}

#pragma mark - GLKView and GLKViewController delegate methods

- (void)update
{
    emulation_step();
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    
}

- (void)didEnterBackground {
    
    GLKView *view = (GLKView *)self.view;
    enterBackGround();
    
    //if (view.active)
        [view resignFirstResponder];
}

- (void)didBecomeActive {
    //if (self.view.active)
        [self.view becomeFirstResponder];
}

- (BOOL)canBecomeFirstResponder {
    return YES;
}

#pragma mark -
#pragma mark UIKeyInput Protocol Methods

- (BOOL)hasText {
    return NO;
}

- (void)insertText:(NSString *)text {
    NSLog(@"Key Input %@\n", text);
}

- (void)deleteBackward {
    // This space intentionally left blank to complete protocol
}


@end
