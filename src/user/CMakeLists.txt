SET(LIB_NAME User)
SET(HEADER_PATH ${user_identification_SOURCE_DIR}/include/${LIB_NAME})
SET(TARGET_H
    ${HEADER_PATH}/User.h
    ${HEADER_PATH}/BaseUserManager.h
    ${HEADER_PATH}/BatchUserManager.h
    ${HEADER_PATH}/StreamUserManager.h
)

SET(TARGET_SRC
	User.cpp
	BaseUserManager.cpp
	BatchUserManager.cpp
	StreamUserManager.cpp
)

# --------------------------------------------
#	Create library
# --------------------------------------------

# link internally (other project libraries)
SET(TARGET_LIBRARIES
	Tracking
	Features
)
# link externally (library paths included over "find_library(${libname})")
SET(TARGET_EXTERNAL_LIBRARIES
		${OpenCV_LIBS}			# OpenCV 3
)

SETUP_LIBRARY(${LIB_NAME})
