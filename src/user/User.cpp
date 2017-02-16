#include <user/User.h>

using namespace user;

// ========= general
void User::ResetUser() {
	mUserID = -1;
	mIDStatus = IDStatus_Unknown;
	mActionStatus = ActionStatus_Idle;
	mTrackingIsSafe = true;
	mUpdatingProfilePicture = false;
	// release profile image
	if (!mProfilePicture.empty())
	{
		mProfilePicture.release();
	}
#ifdef FACEGRID_RECORDING
	// reset face grid
	pGrid->Clear();
#endif
}

// ------ user status
// setters
void User::SetStatus(ActionStatus status) {
	mActionStatus = status;
}
void User::SetStatus(IdentificationStatus status)
{
	mIDStatus = status;
}
void User::SetStatus(TrackingStatus status)
{
	mTrackingStatus = status;
}
void User::SetStatus(HumanTrackingStatus status)
{
	mHumanTrackingStatus = status;
}
// getters
void User::GetStatus(IdentificationStatus &s1, ActionStatus &s2)
{
	s1 = mIDStatus;
	s2 = mActionStatus;
}
void User::GetStatus(ActionStatus &s)
{
	s = mActionStatus;
}
void User::GetStatus(IdentificationStatus &s)
{
	s = mIDStatus;
}
void User::GetStatus(TrackingStatus &s)
{
	s = mTrackingStatus;
}
void User::GetStatus(HumanTrackingStatus &s)
{
	s = mHumanTrackingStatus;
}

// ========= identification
void User::SetUserID(int id, std::string nice_name)
{
	mUserID = id;
	mUserNiceName = nice_name;
	mIDStatus = IDStatus_Identified;
}
void User::SetFaceBoundingBox(cv::Rect2f bb)
{
	mFaceBoundingBox = bb;
}
void User::SetFaceData(tracking::Face f)
{
	// allocate new face
	mFaceData = new tracking::Face(f);
}
cv::Rect2f User::GetFaceBoundingBox()
{
	return mFaceBoundingBox;
}
// ========= features
bool User::GetFaceData(tracking::Face& f)
{
	if (mFaceData == nullptr) {
		return false;
	}
	f = *mFaceData;
	return true;
}

void User::ResetSceneFeatures() {
	// reset all stored features
	if (mFaceData != nullptr) {
		delete(mFaceData);
		mFaceData = nullptr;
	}
}


int User::GetUserID() const
{
	return mUserID;
}

void User::GetUserID(int& id, std::string& nice_name) const
{
	id = mUserID;
	nice_name = mUserNiceName;
}

void User::PrintStatus()
{
	std::cout << "--- id_status: " << mIDStatus << " | action: " << mActionStatus << std::endl;
}
/*
*
// Client Side Picture Taking
- During update (when person has been identified)
- If person has no profile picture yet (nothing received from server)
- Evaluate the face (bb) in each frame if the orientation is frontal
- if it is approx. frontal: optionally rotate
- save picture to user instance
- send profilePictureUpdate Request to server

// difficulties
- profile picture taken, when tracking switches

// solutions
- also classify profile picture and reject if it does not comply with corresponding ID
*/
bool User::IsViewedFromFront()
{
	// get face orientation
	if (mFaceData != nullptr) {
		// calc euler angles
		int roll, pitch, yaw;
		mFaceData->GetEulerAngles(roll, pitch, yaw);
		// optional: rotate image

		if (pitch >= 30 || pitch <= -30)
		{
			return false;
		}
		if (roll >= 30 || roll <= -30)
		{
			return false;
		}
		if (yaw >= 30 || yaw <= -30)
		{
			return false;
		}

		return true;
	}
	return false;
}

bool User::GetProfilePicture(cv::Mat &pic)
{
	if (mProfilePicture.empty())
	{
		return false;
	}
	pic = mProfilePicture;
	return true;
}