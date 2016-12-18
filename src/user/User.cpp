#include <user/User.h>

using namespace user;


void User::SetUserID(int id, std::string nice_name)
{
	mUserID = id;
	mUserNiceName = nice_name;
	mIDStatus = IDStatus_Identified;
}
void User::SetIDStatus(IdentificationStatus status)
{

	mIDStatus = status;
}
void User::SetActionStatus(ActionStatus status) {
	mActionStatus = status;
}
void User::SetFaceBoundingBox(cv::Rect2f bb)
{

	mFaceBoundingBox = bb;
}
void User::SetFaceData(tracking::Face f)
{
	mFaceData = f;
}
void User::GetStatus(IdentificationStatus &s1, ActionStatus &s2)
{
	s1 = mIDStatus;
	s2 = mActionStatus;
}

cv::Rect2f User::GetFaceBoundingBox()
{
	return mFaceBoundingBox;
}
tracking::Face User::GetFaceData()
{
	return mFaceData;
}
