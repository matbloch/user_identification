#include <user\BaseUserManager.h>
#include <user\User.h>
#include <opencv2/imgproc.hpp>
#include <io/RequestHandler.h>
#include <io/RequestTypes.h>
#include <io/ResponseTypes.h>
#include <opencv2/highgui/highgui.hpp>
#include <tracking\FaceTracker.h>

using namespace  user;

bool BaseUserManager::Init(io::TCPClient* connection, io::NetworkRequestHandler* handler)
{
	if (connection == nullptr || handler == nullptr)
	{
		return false;
	}
	pServerConn = connection;
	pRequestHandler = handler;

#ifdef _DLIB_PREALIGN
	// initialize aligner
	mpDlibAligner = new features::DlibFaceAligner();
	mpDlibAligner->Init();
#endif

	std::cout << "--- BaseUserManager initialized" << std::endl;

	return true;
}

void BaseUserManager::UpdateFaceData(std::vector<tracking::Face> faces, std::vector<int> user_ids) {
	for (size_t i = 0; i < faces.size(); i++) {

#ifdef _DEBUG_BaseUserManager
		if(mFrameIDToUser.count(user_ids[i]) == 0)
		{
			throw std::invalid_argument("Updating invalid User!");
		}
#endif

		User* u = mFrameIDToUser[user_ids[i]];
		u->SetFaceData(faces[i]);
	}
}

// refresh tracked users: scene_id, bounding boxes
void BaseUserManager::RefreshUserTracking(
	const std::vector<int> &user_scene_ids, 
	std::vector<cv::Rect2f> bounding_boxes
)
{
	// add new user for all scene ids that are new
	for (int i = 0; i<user_scene_ids.size(); i++)
	{
		int scene_id = user_scene_ids[i];
		// user not tracked yet - initiate new user model
		if (mFrameIDToUser.count(scene_id) == 0)
		{
			// create new user
			mFrameIDToUser[scene_id] = new User(
#ifdef _DLIB_PREALIGN
				mpDlibAligner
#endif
			);
		}
	}

	// update users infos - remove non tracked
	for (auto it = mFrameIDToUser.begin(); it != mFrameIDToUser.end();)
	{
		int user_frame_id = it->first;
		User* target_user = it->second;
		int user_index = find(user_scene_ids.begin(), user_scene_ids.end(), user_frame_id) - user_scene_ids.begin();

		// check if user is in scene
		if (user_index < user_scene_ids.size())
		{
			// user is in scene - update scene data (bounding box, position etc.)
			target_user->UpdateFaceBoundingBox(bounding_boxes[user_index]);
			// reset feature tracking
			target_user->ResetSceneFeatures();
			++it;
		}
		// remove user if he has left scene
		else
		{
			// cancel and unlink all pending requests for a user
			// already processed requests are ignored when response is popped from request handler
			CancelAndDropAllUserRequests(target_user);

			// user has left scene - delete tracking instance
			delete(target_user);

#ifdef _DEBUG_BaseUserManager
			std::cout << "=== User has left scene - removing UserSceneID " << it->first << std::endl;
#endif

			// remove mapping
			mFrameIDToUser.erase(it++);	// increment after deletion
		}
	}

}

void BaseUserManager::CancelAndDropAllUserRequests(User* user) {
	// delete pending requests and delete all linking
	// processed requests without linking are dropped

	// user->requests
	if (mUserToRequests.find(user) != mUserToRequests.end()) {

		// iterate over requests
		for (auto req : mUserToRequests[user]) {
			// unlink: requests->user
			mRequestToUser.erase(req);
			// delete: requests from queue
			pRequestHandler->cancelPendingRequest(req);
		}
		// unlink: user->request
		mUserToRequests.erase(user);
	}
	else {
		// no pending/processed requests found
	}
}


void BaseUserManager::UpdateTrackingStatus() {

	IdentificationStatus ids;

	// check human user tracking status
	for (auto it = mFrameIDToUser.begin(); it != mFrameIDToUser.end(); ++it)
	{
		it->second->GetStatus(ids);

		// update face detection counter
		it->second->IncrementFaceDetectionStatus();

		// increment bounding box movement status
		it->second->IncrementBBMovementStatus();

		// update overall status
		if (ids == IDStatus_IsObject) {
			if (!it->second->IsTrackingObject()) {
				it->second->SetStatus(IDStatus_Unknown);
			}
		}
		else {
			if (it->second->IsTrackingObject()) {
				// reset UserID
				it->second->ResetUserIdentity();
				// set to object
				it->second->SetStatus(IDStatus_IsObject);
			}
		}


	}

	// check tracking confidence
#ifdef _CHECK_TRACKING_CONF
	std::map<int, bool> scene_ids_uncertain;

	// get current tracking status
	for (auto uit1 = mFrameIDToUser.begin(); uit1 != mFrameIDToUser.end(); ++uit1)
	{
		// choose pair (if not last element)
		if (uit1 != std::prev(mFrameIDToUser.end())) {
			for (auto it2 = std::next(uit1); it2 != mFrameIDToUser.end(); ++it2) {
				cv::Rect r1 = uit1->second->GetFaceBoundingBox();
				cv::Rect r2 = it2->second->GetFaceBoundingBox();

				// bbs intersect if area > 0
				bool intersect = ((r1 & r2).area() > 0);
				if (intersect) {
					// track scene ids
					scene_ids_uncertain[uit1->first] = true;
					scene_ids_uncertain[it2->first] = true;
				}
			}
		}
	}

	for (auto it = mFrameIDToUser.begin(); it != mFrameIDToUser.end(); ++it)
	{
		IdentificationStatus s;
		ActionStatus as;
		it->second->GetStatus(s);
		it->second->GetStatus(as);

		// currently unsafe tracking
		if(scene_ids_uncertain.count(it->first) > 0)
		{
			// update temp tracking status
			it->second->SetStatus(TrackingConsistency_Uncertain);

			// tracking is uncertain atm
			if(s==IDStatus_Identified)
			{
				// update id status
				it->second->SetStatus(IDStatus_Uncertain);
				// cancel all pending requests
				// atm: no robust request besides reidentification
				// todo: else cancel all other requests here (prevent delayed reidentification at an unsafe state)

				// wait till tracking is save again, then do reidentification
				it->second->SetStatus(ActionStatus_WaitForCertainTracking);
			}
			else if(s == IDStatus_Uncertain) {


				// cancel possible pending reidentification requests
				if (as == ActionStatus_Waiting) {
					CancelAndDropAllUserRequests(it->second);
				}

				// wait till tracking is save again, then do reidentification
				it->second->SetStatus(ActionStatus_WaitForCertainTracking);

			}
			else if (s == IDStatus_Unknown) {
				// cancel data collection - unambiguous
				if (as == ActionStatus_Waiting) {
					CancelAndDropAllUserRequests(it->second);
				}

				it->second->SetStatus(ActionStatus_WaitForCertainTracking);
			}
		// safe tracking
		}else
		{
			// update temp tracking status
			it->second->SetStatus(TrackingConsistency_OK);

			// tracking safe again - reset samples and start data collection
			if(s==IDStatus_Uncertain && as==ActionStatus_WaitForCertainTracking)
			{
				it->second->SetStatus(ActionStatus_Idle);
			}
			else if (s == IDStatus_Unknown && as == ActionStatus_WaitForCertainTracking) {
				it->second->SetStatus(ActionStatus_Idle);
			}
		}

	}
#endif

}


// ----------------- API functions

std::vector<std::pair<int, int>> BaseUserManager::GetUserandTrackingID() {
	std::vector<std::pair<int, int>> out;
	for (auto it = mFrameIDToUser.begin(); it != mFrameIDToUser.end(); ++it)
	{
		out.push_back(std::make_pair(it->second->GetUserID(), it->first));
	}
	return out;
}

std::vector<std::pair<int, cv::Mat>> BaseUserManager::GetSceneProfilePictures() {
	std::vector<std::pair<int, cv::Mat>> out;
	cv::Mat profile_pic;
	for (auto it = mFrameIDToUser.begin(); it != mFrameIDToUser.end(); ++it)
	{
		if (it->second->GetProfilePicture(profile_pic)) {
			out.push_back(std::make_pair(it->second->GetUserID(), profile_pic));
		}
	}
	return out;
}

std::vector<std::pair<int, cv::Mat>> BaseUserManager::GetAllProfilePictures() {
	std::vector<std::pair<int, cv::Mat>> out;
	// request images from server
	io::GetProfilePictures req(pServerConn);
	pServerConn->Connect();
	req.SubmitRequest();
	// wait for reponse
	io::ProfilePictures resp(pServerConn);
	int response_code = 0;
	if (!resp.Load(&response_code)) {
		// error
	}
	else {
#ifdef _DEBUG_BaseUserManager
		if (resp.mUserIDs.size() != resp.mUserIDs.size()) {
			std::cout << "--- Error: size(user_ids) != size(user_profile_pictures)!\n";
		}
#endif
		// load images
		for (size_t i = 0; i < resp.mUserIDs.size(); i++) {
			out.push_back(std::make_pair(resp.mUserIDs[i], resp.mImages[i]));
		}
	}
	pServerConn->Close();
	return out;
}

void BaseUserManager::GetAllProfilePictures(std::vector<cv::Mat> &pictures, std::vector<int> &user_ids) {
	// request images from server
	io::GetProfilePictures req(pServerConn);
	pServerConn->Connect();
	req.SubmitRequest();
	// wait for reponse
	io::ProfilePictures resp(pServerConn);
	int response_code = 0;
	if (!resp.Load(&response_code)) {
		// error
	}
	else {
#ifdef _DEBUG_BaseUserManager
		if (resp.mUserIDs.size() != resp.mUserIDs.size()) {
			std::cout << "--- Error: size(user_ids) != size(user_profile_pictures)!\n";
		}
#endif
		// load images
		for (size_t i = 0; i < resp.mUserIDs.size(); i++) {
			user_ids.push_back(resp.mUserIDs[i]);
			pictures.push_back(resp.mImages[i]);
		}
	}
	pServerConn->Close();
}

bool BaseUserManager::GetUserID(const cv::Mat &face_capture, int &user_id) {
	std::vector<cv::Mat> face_patches = { face_capture };
	IDReq id_request(pServerConn, face_patches);
	pServerConn->Connect();
	id_request.SubmitRequest();
	user_id = -1;
	bool succ = false;
	// wait for reponse
	io::IdentificationResponse id_response(pServerConn);
	int response_code = 0;
	if (!id_response.Load(&response_code)) {
		//std::cout << "--- An error occurred during update: ResponseType " << response_code << " | expected: " << id_response.cTypeID << std::endl;
	}
	else {
		succ = true;
		user_id = id_response.mUserID;
	}
	pServerConn->Close();
	return succ;
}

// ----------------- helper functions

void BaseUserManager::DrawUsers(cv::Mat &img)
{
	for (auto it = mFrameIDToUser.begin(); it != mFrameIDToUser.end(); ++it)
	{
		user::User* target_user = it->second;
		
		cv::Rect bb = target_user->GetFaceBoundingBox();

		// render user profile image
		cv::Mat profile_image;
		if (target_user->GetProfilePicture(profile_image))
		{
			cv::resize(profile_image, profile_image, cv::Size(100, 100));

			if(bb.y > 0 && bb.x < img.cols)
			{
				// check if roi overlapps image borders
				cv::Rect target_roi = cv::Rect(bb.x, bb.y - profile_image.rows, profile_image.cols, profile_image.rows);
				cv::Rect src_roi = cv::Rect(0, 0, profile_image.cols, profile_image.rows);

				if(target_roi.y < 0)
				{
					src_roi.y = -target_roi.y;
					src_roi.height += target_roi.y;
					target_roi.height += target_roi.y;
					target_roi.y = 0;
				}

				if (target_roi.x + profile_image.cols > img.cols)
				{
					src_roi.width = target_roi.x + profile_image.cols - img.cols;
					target_roi.width = target_roi.x + profile_image.cols - img.cols;
				}

				try {
					profile_image = profile_image(src_roi);
					profile_image.copyTo(img(target_roi));
				}
				catch (...) {
					// ...
				}

			}

			// render inside bb
			if(false)
			{
				cv::Rect picture_patch = cv::Rect(bb.x, bb.y, profile_image.cols, profile_image.rows);
				profile_image.copyTo(img(picture_patch));
			}
		}

		// draw identification status
		float font_size = 0.5;
		cv::Scalar color = cv::Scalar(0, 0, 0);
		cv::Scalar bg_color = cv::Scalar(0, 0, 0);
		std::string text1, text2;

		IdentificationStatus id_status;
		ActionStatus action;
		target_user->GetStatus(id_status, action);

		if (id_status == IDStatus_Identified || id_status == IDStatus_Uncertain)
		{
			int user_id = 0;
			std::string nice_name = "";
			target_user->GetUserID(user_id, nice_name);
			text1 = "Status: ID" + std::to_string(user_id);
			//text1 = "Status: " + nice_name + " - ID" + std::to_string(user_id);
			color = cv::Scalar(0, 255, 0);

			// confidence


			if (id_status == IDStatus_Uncertain) {
				if (action == ActionStatus_WaitForCertainTracking) {
					text1 += " | waiting for safe tracking";
				}
				else if (action == ActionStatus_Waiting) {
					text1 += " | pending reidentification";
				}
				else if (action == ActionStatus_DataCollection) {
					text1 += " | sampling (" + std::to_string(target_user->pGrid->nr_images()) + ")";
				}
			}

		}
		else if (id_status == IDStatus_IsObject) {
			text1 = "OBJECT TRACKING";
			color = cv::Scalar(0, 0, 0);
			bg_color = cv::Scalar(0, 0, 255);
		}
		else
		{
			text1 = "Status: unknown";
			if (action == ActionStatus_DataCollection) {
				text2 = "Sampling ("+std::to_string(target_user->pGrid->nr_images())+")";
			}
			else if (action == ActionStatus_Waiting) {
				text2 = "ID pending";
			}
			else if (action == ActionStatus_Idle) {
				text2 = "Idle";
			}
			else if (action == ActionStatus_WaitForCertainTracking) {
				text2 = "Wait for safe tracking";
			}

			text2 += " " + target_user->GetHumanStatusString();

			color = cv::Scalar(0, 0, 255);
		}

		int baseline = 0;
		cv::Size textSize = cv::getTextSize(text1, cv::FONT_HERSHEY_SIMPLEX, font_size, 1, &baseline);
		cv::Rect bg_patch = cv::Rect(bb.x, bb.y, textSize.width + 20, textSize.height + 15);

		user::TrackingConsistency tracking_status;
		target_user->GetStatus(tracking_status);
		if (tracking_status == user::TrackingConsistency_Uncertain) {
			bg_color = cv::Scalar(0, 14, 88);
		}

		// draw flat background
		img(bg_patch) = bg_color;

		cv::putText(img, text1, cv::Point(bb.x+10, bb.y+20), cv::FONT_HERSHEY_SIMPLEX, font_size, color, 1, 8);
		cv::putText(img, text2, cv::Point(bb.x+10, bb.y+40), cv::FONT_HERSHEY_SIMPLEX, font_size, color, 1, 8);

		// draw face bounding box
		if (id_status == IDStatus_Uncertain) {
			cv::rectangle(img, bb, cv::Scalar(0, 14, 88), 2, cv::LINE_4);
		}
	}
}
