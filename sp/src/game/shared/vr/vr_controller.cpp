#include "cbase.h"
#include "vr/vr_controller.h"

#include <winlite.h>

#include <sixense.h>
#include <sixense_math.hpp>
#include <sixense_utils/interfaces.hpp>
#include <sixense_utils/button_states.hpp>
#include <sixense_utils/controller_manager/controller_manager.hpp>
#include <sixense_utils/keyboard_and_mouse_win32.hpp>

#include "vgui/IVGui.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Label.h"
#include "ienginevgui.h"
#include "vgui_controls/ImagePanel.h"
#include "c_basehlplayer.h"
#include "usermessages.h"


// user message handler func for suit power so it's available to the client, no idea why this wasn't already there....
void __MsgFunc_CLocalPlayer_Battery(bf_read &msg) 
{						
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer*>( C_BasePlayer::GetLocalPlayer() );
	if ( pPlayer )
	{
		pPlayer->SetSuitArmor(Clamp(msg.ReadShort(), 0, 100));
	}
}


using sixenseMath::Vector2;
using sixenseMath::Vector3;
using sixenseMath::Vector4;
using sixenseMath::Quat;
using sixenseMath::Line;

void mt_calibrate_f( void )
{
    Msg("Beginning Motion Tracker Calibration.... \n");
	g_MotionTracker()->beginCalibration();
}
ConCommand mt_calibrate( "mt_calibrate", mt_calibrate_f, "Shows a message.", 0 );

static ConVar mt_torso_movement_scale( "mt_torso_movement_scale", "1", FCVAR_ARCHIVE, "Scales positional tracking for torso tracker");
static ConVar mt_right_hand_movement_scale( "mt_right_hand_movement_scale", "1", FCVAR_ARCHIVE, "Scales positional tracking for right hand");
static ConVar mt_control_mode( "mt_control_mode", "2", FCVAR_ARCHIVE, "Sets the hydra control mode: 0 = Hydra in each hand, 1 = Right hand and torso, 2 = Right hand and torso with augmented one-handed controls");
static ConVar mt_swap_hydras( "mt_swap_hydras", "0", 0, "Flip the right & left hydras");
static ConVar mt_menu_control_mode( "mt_menu_control_mode", "0", FCVAR_ARCHIVE, "Control the mouse in menu with 0 = Right joystick, 1 = Right hand position, 2 = Both");
static ConVar mt_tactical_haptics( "mt_tactical_haptics", "0", FCVAR_ARCHIVE, "Special mode for the hydra orientation needed for the tactical haptics guys' setup");
static ConVar mt_calibration_offset_down( "mt_calibration_offset_down", "15", FCVAR_ARCHIVE, "Downward offset for calibration position (higher pushes weapon further down)");
static ConVar mt_calibration_offset_forward( "mt_calibration_offset_forward", "0", FCVAR_ARCHIVE, "Forward offset for calibration position (higher moves weapon further forward per the same hand position)");

MotionTracker* _motionTracker;

#define MM_TO_INCHES(x) (x/1000.f)*(1/METERS_PER_INCH)

extern MotionTracker* g_MotionTracker() { 	return _motionTracker; }


// A bunch of sixense integration crap that needs to be pulled out along w/ vr.io`
matrix3x4_t sixenseToSourceMatrix( sixenseMath::Matrix4 ss_mat )
{
	sixenseMath::Matrix4 tmp_mat( ss_mat );

	tmp_mat = tmp_mat * sixenseMath::Matrix4::rotation( -3.1415926f / 2.0f, Vector3( 1, 0, 0 ) );
	tmp_mat = tmp_mat * sixenseMath::Matrix4::rotation( -3.1415926f / 2.0f, Vector3( 0, 0, 1 ) );
	tmp_mat = tmp_mat * sixenseMath::Matrix4::rotation( 3.1415926f, Vector3( 0, 1, 0 ) );
	tmp_mat = sixenseMath::Matrix4::rotation( -3.1415926f / 2.0f, Vector3( 1, 0, 0 ) ) * tmp_mat;
	tmp_mat = sixenseMath::Matrix4::rotation( -3.1415926f / 2.0f, Vector3( 0, 1, 0 ) ) * tmp_mat;
		
	matrix3x4_t retmat;

	MatrixInitialize( 
		retmat, 
		Vector( tmp_mat[3][0], tmp_mat[3][1], tmp_mat[3][2] ),
		Vector( tmp_mat[1][0], tmp_mat[1][1], tmp_mat[1][2] ),
		Vector( tmp_mat[2][0], tmp_mat[2][1], tmp_mat[2][2] ),
		Vector( tmp_mat[0][0], tmp_mat[0][1], tmp_mat[0][2] )
	);

	return retmat;
}

matrix3x4_t getMatrixFromData(sixenseControllerData m) {
	sixenseMath::Matrix4 matrix;

	matrix = sixenseMath::Matrix4::rotation( sixenseMath::Quat( m.rot_quat[0], m.rot_quat[1], m.rot_quat[2], m.rot_quat[3] ) );
	Vector3 ss_left_pos = sixenseMath::Vector3( 
								MM_TO_INCHES(m.pos[0]), 
								MM_TO_INCHES(m.pos[1]), 
								MM_TO_INCHES(m.pos[2])
							);
	
	matrix.set_col( 3, sixenseMath::Vector4( ss_left_pos, 1.0f ) );

	return sixenseToSourceMatrix(matrix);
}


MotionTracker::MotionTracker()
{
	Msg("Initializing Motion Tracking API");
	
	_baseEngineYaw = 0;
	_prevYawTorso = 0;
	_accumulatedYawTorso = 0;
	_counter = 0;
	_calibrate = true;
	_initialized = false;
	_isGuiActive = false;
	_strafeModifier = false;
	_rightBumperPressed = false;
	_motionTracker = this;
	_sixenseCalibrationNeeded = false;
	_previousHandPosition.Init();
	_previousHandAngle.Init();

	sixenseInitialize();

	_rhandCalibration.Identity(); // _rhandCalibration is the calibration between sixense and the torso coordinate space
	
	PositionMatrix(Vector(-1, 0, -11.5), _eyesToTorsoTracker);
	
	_controlMode = (MotionControlMode_t) mt_control_mode.GetInt();

	usermessages->HookMessage("Battery", __MsgFunc_CLocalPlayer_Battery ); // while this has nothing really to do with this code, it's VR only
} 

MotionTracker::~MotionTracker()
{
	shutDown();
}

void MotionTracker::shutDown()
{
	if (_initialized)
	{
		Msg("Shutting down VR Controller");
		_initialized = false;
		sixenseShutdown();
	}
	else 
	{
		Msg("VR Controller already shut down, nothing to do here.");
	}
}


void printVec(Vector v)
{
	Msg("x:%.2f y:%.2f z:%.2f\n", v.x, v.y, v.z);
}
void printVec(float* v)
{
	Msg("x:%.2f y:%.2f z:%.2f\n", v[0], v[1], v[2]);
}

bool MotionTracker::isTrackingWeapon( )
{
	return _initialized;
}

bool MotionTracker::isTrackingTorso( )
{
	return true; // todo: check for stem tracker...
}

matrix3x4_t MotionTracker::getTrackedTorso()
{	
	return getMatrixFromData(getControllerData(sixenseUtils::ControllerManager::P1L));
}

matrix3x4_t MotionTracker::getTrackedRightHand(bool includeCalibration)
{
	matrix3x4_t m = getMatrixFromData(getControllerData(sixenseUtils::ControllerManager::P1R));

	if ( mt_tactical_haptics.GetBool() )
	{
		VMatrix tmp(m);
		tmp.SetBasisVectors(-tmp.GetUp(), -tmp.GetLeft(), -tmp.GetForward()); 
		return tmp.As3x4();
	}
	
	return m;
}

void MotionTracker::updateViewmodelOffset(Vector& vmorigin, QAngle& vmangles)
{
	if ( !_initialized )
		return;

	matrix3x4_t weaponMatrix	= getTrackedRightHand();
	matrix3x4_t torsoMatrix		= getTrackedTorso();
	
	Vector weaponPos;

	// get raw torso and weap positions, construct the distance from the diff of those two + the distance to the eyes from a properly calibrated torso tracker...
	Vector vWeapon, vEyes;
	MatrixPosition(weaponMatrix, vWeapon);
	MatrixPosition(_eyesToTorsoTracker, vEyes);

	// weaponMatrix is in sixense coordinate space
	// torsoMatrix is in sixense coordinate space
	
	VectorRotate(vEyes, _sixenseToWorld, vEyes);
	VectorRotate(vEyes, _rhandCalibration.As3x4(), vEyes);
	
	// TODO: still getting weird behavior with the eye forward offset
	// it is converted into the calibrated sixense space, and is then combined with the weapon offset (not yet in calibrated space)
	// and is then

	Vector vEyesToWeapon = (vWeapon - _vecBaseToTorso)  + vEyes;			// was ( weapon - torso ) but since at this point the torso changes haven't been applied (get overridden in the view), that's unnecessary...
	
	PositionMatrix(vEyesToWeapon, weaponMatrix);							// position is reset rather than distance to base to distance to torso tracker
	
	MatrixMultiply(_sixenseToWorld, weaponMatrix, weaponMatrix);			// _sixenseToWorld converts to torso relative
	MatrixMultiply(_rhandCalibration.As3x4(), weaponMatrix, weaponMatrix);  // adjust the weapon angles per the calibration

	MatrixPosition(weaponMatrix, weaponPos);								// get the angles & positions back off
	MatrixAngles(weaponMatrix, vmangles);									
		
	vmorigin += weaponPos;
}

void MotionTracker::getEyeToWeaponOffset(Vector& offset)
{
	if ( !_initialized ) 
	{
		offset.Init();
		return;
	}

	matrix3x4_t weaponMatrix = getTrackedRightHand();
		
	Vector vWeapon, vEyes;
	MatrixPosition(weaponMatrix, vWeapon);
	MatrixPosition(_eyesToTorsoTracker, vEyes);
	// TODO same issue here as before, eyeToTorso must be adjusted to torso coordinates in game for eye offset to work properly
	
	offset = (vWeapon - _vecBaseToTorso)  + vEyes;				

	PositionMatrix(offset, weaponMatrix);							// position is reset rather than distance to base to distance to torso tracker
	MatrixMultiply(_sixenseToWorld, weaponMatrix, weaponMatrix);	// convert from sixense to torso coordinates
	MatrixMultiply(_rhandCalibration.As3x4(), weaponMatrix, weaponMatrix);  // from torso to adjusted... todo: just for clarity, it should be sixenseToCalibrated -> calibratedToTorso
	MatrixPosition(weaponMatrix, offset);							// get the position back off
}


void MotionTracker::getEyeToLeftHandOffset(Vector& offset)
{
	if ( !_initialized ) 
	{
		offset.Init();
		return;
	}

	matrix3x4_t leftHandMatrix = getTrackedTorso(); // todo: shoud b
		
	Vector vWeapon, vEyes;
	MatrixPosition(leftHandMatrix, vWeapon);
	MatrixPosition(_eyesToTorsoTracker, vEyes);
	// TODO same issue here as before, eyeToTorso must be adjusted to torso coordinates in game for eye offset to work properly
	
	offset = (vWeapon - _vecBaseToTorso)  + vEyes;				

	PositionMatrix(offset, leftHandMatrix);					// position is reset rather than distance to base to distance to torso tracker
	MatrixMultiply(_sixenseToWorld, leftHandMatrix, leftHandMatrix);	// convert from sixense to torso coordinates
	MatrixMultiply(_rhandCalibration.As3x4(), leftHandMatrix, leftHandMatrix);  // from torso to adjusted... todo: just for clarity, it should be sixenseToCalibrated -> calibratedToTorso
	MatrixPosition(leftHandMatrix, offset);							// get the position back off
}







void MotionTracker::getViewOffset(Vector& offset)
{
	if ( !_initialized || _controlMode == TRACK_BOTH_HANDS  )
	{
		offset.Init();
		return;
	}

	// todo: need to clean this up
	matrix3x4_t torsoMatrix = getTrackedTorso();
	MatrixPosition(torsoMatrix, offset);
	offset -= _vecBaseToTorso;
	PositionMatrix(offset, torsoMatrix);
	MatrixMultiply(_sixenseToWorld, torsoMatrix, torsoMatrix);
	MatrixPosition(torsoMatrix, offset);
}


void MotionTracker::overrideViewOffset(VMatrix& viewMatrix)
{
	if ( !_initialized || _controlMode == TRACK_BOTH_HANDS  )
		return;

	Vector offset;
	getViewOffset(offset);
	Vector viewTranslation = viewMatrix.GetTranslation();
	viewTranslation += offset;
	viewMatrix.SetTranslation(viewTranslation);
}

QAngle MotionTracker::getTorsoAngles()
{
	float yaw = _baseEngineYaw;

	// left hand orientation can be disregarded when it's just being used as a controller.
	if ( _controlMode != TRACK_BOTH_HANDS )
		yaw += _accumulatedYawTorso;
	
	return QAngle(0, yaw, 0);
}

void MotionTracker::overrideWeaponMatrix(VMatrix& weaponMatrix)
{
	if ( !_initialized )
		return;

	QAngle weaponAngle;
	MatrixToAngles(weaponMatrix, weaponAngle);
	Vector weaponOrigin = weaponMatrix.GetTranslation();	
		
	// for now let's just reuse the overrideviewmodel stuff that does exactly the same thing (from mideye view to weapon position & orientation..)
	updateViewmodelOffset(weaponOrigin, weaponAngle);

	MatrixFromAngles(weaponAngle, weaponMatrix);
	weaponMatrix.SetTranslation(weaponOrigin);
}

void MotionTracker::overrideMovement(Vector& movement)
{
	if ( !_initialized || _controlMode == TRACK_BOTH_HANDS  )
		return;
	
	QAngle angle;
	VectorAngles(movement, angle);
	float dist = movement.Length();

	angle.x = 0;
	angle.z = 0;
	angle.y -= _accumulatedYawTorso;
	
	AngleVectors(angle, &movement);
	movement.NormalizeInPlace();
	movement*=dist;
}

// Update uses the inbound torso (view if no rift) angles and uses them update / the base matrix that should be applied to sixense inputs....
void MotionTracker::update(VMatrix& torsoMatrix)
{
	if ( !_initialized )
		return;
	
	sixenseUpdate();

	if ( (_counter % 20) == 0 ) // no need to do this every frame
		_controlMode = (MotionControlMode_t) mt_control_mode.GetInt();

	if ( !isTrackingTorso() )
		return;
	
	if ( _calibrate )
		calibrate(torsoMatrix);

	QAngle torsoAngle;
	MatrixToAngles(torsoMatrix, torsoAngle);

	QAngle trackedTorsoAngles; 
	MatrixAngles(getTrackedTorso(), trackedTorsoAngles);
	
	_accumulatedYawTorso += ( trackedTorsoAngles.y - _prevYawTorso );
	_prevYawTorso = trackedTorsoAngles.y;
	_baseEngineYaw = torsoAngle.y;
	
	AngleMatrix(QAngle(0, _baseEngineYaw, 0), _sixenseToWorld);

	_counter++;
}

void MotionTracker::beginCalibration() { _calibrate = true; }
void MotionTracker::calibrate(VMatrix& torsoMatrix)
{
	if ( !_initialized )
		return;
	
	QAngle engineTorsoAngles; 
	MatrixToAngles(torsoMatrix, engineTorsoAngles);
	_baseEngineYaw = engineTorsoAngles.y;
	_accumulatedYawTorso = 0; // only used for movement vector adjustments...

	// regardless of control mode, we snapshot the torso (lhand) tracker offset, the only change is how it's applied in the viewmodel offsets...
	matrix3x4_t trackedTorso = getTrackedTorso();
	MatrixGetTranslation(trackedTorso, _vecBaseToTorso);
		
	if ( _controlMode == TRACK_BOTH_HANDS )
	{
		
		// With both hands, best way to calibrate is to have hands at shoulders
		// and infer forward as perpendicular to the vector between the controllers
		// removing the need to know the base station alignment

		matrix3x4_t matRightHand = getTrackedRightHand();
		Vector rightHand, leftHand, rightHandToLeft;
		leftHand = _vecBaseToTorso;
		MatrixGetTranslation(matRightHand, rightHand);
		rightHandToLeft = leftHand - rightHand;
		
		Vector forward;
		CrossProduct(rightHandToLeft.Normalized(), Vector(0,0,1), forward);
		forward.z = 0;
				
		VectorMatrix(forward, _rhandCalibration.As3x4());
		_rhandCalibration = _rhandCalibration.InverseTR();

		// Now we reset vecBaseToTorso to be the midpoint between the two
		_vecBaseToTorso = rightHand + rightHandToLeft/2.f + forward*-mt_calibration_offset_forward.GetFloat();

		// And we should also adjust the expected offset from head to "torso" b/c it's a bit harder to set perfectly otherwise..
		
		PositionMatrix(Vector(0, 0, -mt_calibration_offset_down.GetFloat()), _eyesToTorsoTracker);

	}

	_lastCalibrated = gpGlobals->curtime;
	_calibrate = false;
}


void MotionTracker::overrideJoystickInputs(float& lx, float& ly, float& rx, float& ry)
{
	if ( !_initialized )
		return;

	sixenseControllerData rhand = getControllerData( sixenseUtils::ControllerManager::P1R );
	sixenseControllerData lhand = getControllerData( sixenseUtils::ControllerManager::P1L );
	
	ry =  rhand.joystick_y * 32766;
	rx =  rhand.joystick_x * 32766;
	
	if ( _controlMode == TRACK_BOTH_HANDS )
	{
		ly = -lhand.joystick_y * 32766;
		lx =  lhand.joystick_x * 32766;
	}
	
	// CUSTOM ONE HYDRA MODE
	if ( _controlMode == TRACK_RHAND_TORSO_CUSTOM )
	{

		// Right hand up/down always moves forward/backward
		ly =  -rhand.joystick_y * 32766; 

		// If not shifted we do strafe rather than turn
		if ( !_rightBumperPressed ) 
		{
			lx =  rhand.joystick_x * 32766;
			rx = 0;
		}
	}
}


void MotionTracker::sixenseInitialize()
{
	Msg("Initializing Sixense Controllers\n");

	int rc = 0;
	sixenseInit();
	
	if ( rc == SIXENSE_FAILURE )
		return;

	int trys = 0;
	int base_found = 0;
		
	while (trys < 3 && base_found == 0) {
		base_found = sixenseIsBaseConnected(0);
		if ( base_found == 0 ) {
			Msg("Sixense base not found on try %d\n", trys);
			trys++;
			Sleep(1000);
		}
	}

	if ( base_found == 0 )
		return;
	
	rc = sixenseSetActiveBase(0);
	
	_sixenseControllerData = new _sixenseAllControllerData();
	_leftButtonStates = new sixenseUtils::ButtonStates();
	_rightButtonStates = new sixenseUtils::ButtonStates();
	_controllerManager = sixenseUtils::getTheControllerManager();

	_controllerManager->setGameType(sixenseUtils::IControllerManager::ONE_PLAYER_TWO_CONTROLLER);


	_initialized = true;
	Msg("Sixense Initialization Complete\n");

}

void MotionTracker::sixenseShutdown()
{
	sixenseExit();
}


static void updateSixenseKey( sixenseUtils::IButtonStates* state, unsigned short sixenseButton, char key )
{
	sixenseUtils::mouseAndKeyboardWin32 keyboard;

	if ( state->buttonJustPressed(sixenseButton) ) 
	{
		keyboard.sendKeyState(key, 1, 0);
	}
	else if ( state->buttonJustReleased(sixenseButton) )
	{
		keyboard.sendKeyState(key, 0, 1);
	}
}

static void updateSixenseTrigger( sixenseUtils::IButtonStates* state, char key)
{
	sixenseUtils::mouseAndKeyboardWin32 keyboard;

	if ( state->triggerJustPressed() ) 
	{
		keyboard.sendKeyState(key, 1, 0);
	} 
	else if (state->triggerJustReleased() ) 
	{
		keyboard.sendKeyState(key, 0, 1);
	}
}

static void updateSixenseTrigger( sixenseUtils::IButtonStates* state) // for mouse click
{
	sixenseUtils::mouseAndKeyboardWin32 keyboard;

	if ( state->triggerJustPressed() ) 
	{
		keyboard.sendMouseClick(1,0);
	} 
	else if (state->triggerJustReleased() ) 
	{
		keyboard.sendMouseClick(0,1);
	}
}


void MotionTracker::sixenseUpdate()
{
	if ( !_initialized )
		return;

	sixenseGetAllNewestData(_sixenseControllerData);
	_controllerManager->update(_sixenseControllerData);	
	
	sixenseControllerData leftController = getControllerData(sixenseUtils::ControllerManager::P1L);
	sixenseControllerData rightController = getControllerData(sixenseUtils::ControllerManager::P1R);

	if ( leftController.which_hand == 0 || rightController.which_hand == 0 )
	{
		if ( writeDebug() )
			Warning("Place your hydras on the base station!\n");
		
		_sixenseCalibrationNeeded = true;
	}
	else 
	{
		_sixenseCalibrationNeeded = false;		
	}

	_leftButtonStates->update( &leftController );
	_rightButtonStates->update( &rightController );
	
	if ( _controlMode == TRACK_RHAND_TORSO_CUSTOM )
		sixenseMapKeysCustom();
	else
		sixenseMapKeys();
	
	sixenseGuiMouseControl();	
}


void MotionTracker::sixenseMapKeys()
{
	if ( _controlMode == TRACK_BOTH_HANDS ) 
	{
		// ONLY BIND LEFT STICK IF IT'S BEING HELD, SHOULD HELP WITH ACCIDENTAL PRESSES
		updateSixenseTrigger( _leftButtonStates, KEY_LCONTROL);
		updateSixenseKey( _leftButtonStates, SIXENSE_BUTTON_START,		KEY_ESCAPE );
		updateSixenseKey( _leftButtonStates, SIXENSE_BUTTON_1,			KEY_Q );
		updateSixenseKey( _leftButtonStates, SIXENSE_BUTTON_2,			KEY_F );
		updateSixenseKey( _leftButtonStates, SIXENSE_BUTTON_3,			KEY_G );
		updateSixenseKey( _leftButtonStates, SIXENSE_BUTTON_4,			KEY_I );
		updateSixenseKey( _leftButtonStates, SIXENSE_BUTTON_JOYSTICK,	KEY_LSHIFT );
		updateSixenseKey( _leftButtonStates, SIXENSE_BUTTON_BUMPER,		KEY_SPACE );
	}

	updateSixenseTrigger( _rightButtonStates);
	updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_START,		KEY_P );
	updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_1,			KEY_R );
	updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_2,			KEY_U );
	updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_3,			KEY_E );
	updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_4,			KEY_J );
	updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_BUMPER,	KEY_Z);
	updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_JOYSTICK,	KEY_C );
}


void MotionTracker::sixenseMapKeysCustom()
{

	// right bumper used for shift state
	if ( _rightButtonStates->buttonJustPressed(SIXENSE_BUTTON_BUMPER) ) 
		_rightBumperPressed = true;
	else if ( _rightButtonStates->buttonJustReleased(SIXENSE_BUTTON_BUMPER) )
		_rightBumperPressed = false;
	
	// normal mode
	if ( !_rightBumperPressed )
	{
		updateSixenseTrigger( _rightButtonStates); // fire
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_START,		KEY_P ); 		// calibrate
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_1,			KEY_LCONTROL ); 	// sprint
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_2,			KEY_E ); 		// use
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_3,			KEY_LSHIFT );	// duck
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_4,			KEY_J );		// weapon cycle
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_JOYSTICK,	KEY_SPACE );	// jump
	}
	else 
	{
		updateSixenseTrigger( _rightButtonStates, KEY_Z); // secondary fire
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_START,		KEY_P ); 		// still calibrate
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_1,			KEY_R ); 		// reload
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_2,			KEY_U ); 		// weapon cycle up 
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_3,			KEY_E );		// still use?
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_4,			KEY_J );		// weapon cycle down
		updateSixenseKey( _rightButtonStates, SIXENSE_BUTTON_JOYSTICK,	KEY_F ); 		// flashlight
	}
}


void MotionTracker::sixenseGuiMouseControl()
{
	sixenseControllerData rightController = getControllerData(sixenseUtils::ControllerManager::P1R);
	sixenseUtils::mouseAndKeyboardWin32 mouseKeyboard;

	matrix3x4_t hand = getTrackedRightHand();
	Vector handPos;
	MatrixPosition(hand, handPos);

	if (( enginevgui && enginevgui->IsGameUIVisible() ) || vgui::surface()->IsCursorVisible() ) {
	
		// move mouse to visible corner if first frame in gui
		if ( !_isGuiActive ) {
			vgui::input()->SetCursorPos( ScreenWidth()/2, ScreenHeight()/2);
		}
		
		int menuControlMode = mt_menu_control_mode.GetInt();
		Vector mouseMove(0,0,0);

		if (( menuControlMode == RIGHT_HAND_POS || menuControlMode == BOTH ) && !_rightBumperPressed )
		{
			mouseMove += (handPos - _previousHandPosition) * 130;
		}

		if ( menuControlMode == RIGHT_JOYSTICK || menuControlMode == BOTH )
		{
			mouseMove.y -= rightController.joystick_x * 6.5;
			mouseMove.z += rightController.joystick_y * 6.5;
		}
		
		mouseKeyboard.sendRelativeMouseMove(-mouseMove.y, mouseMove.z);
		_isGuiActive = true;
	} 
	else if ( _isGuiActive )
	{
		_isGuiActive = false;
	}

	VectorCopy(handPos, _previousHandPosition);
}


sixenseControllerData MotionTracker::getControllerData(sixenseUtils::IControllerManager::controller_desc which_controller)
{
	int idx = _controllerManager->getIndex( which_controller );
	
	if ( idx < 0 || idx > 1 )
	{
		idx = (int) which_controller;
	}

	if ( mt_swap_hydras.GetBool() )
		idx = (idx+1) % 2;
	
	return _sixenseControllerData->controllers[idx];
}


bool MotionTracker::writeDebug() {
	return (_counter % 120) == 0;
}


float MotionTracker::getHudPanelAlpha(const Vector& hudPanelForward, const Vector& eyesForward, float fadePow)
{
	float dot = hudPanelForward.Dot(eyesForward);
	if ( dot > 0 ) return 0.f;
	return pow(fabs(dot), fadePow);
}


bool MotionTracker::showMenuPanel()
{
	return _sixenseCalibrationNeeded;
}
