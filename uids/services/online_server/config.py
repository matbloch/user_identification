#!/usr/bin/env python

"""Global server configuration - Request/Response names match models in src/request src/response"""
ROUTING = {
   "REQUEST": {  # request id range: 0-255
       "ID": {
           "ImageIdentification": 1,  # identify user from image
           "ImageIdentificationUpdate": 2,  # collect embeddings from images for user with ID XY
           "ImageIdentificationAligned": 6,
           "ImageIdentificationUpdateAligned": 7,
           "ImageAlignment": 8,
           "ProfilePictureUpdate": 9
       },
       "NAME": {
           1: "ImageIdentification",
           2: "ImageIdentificationUpdate",
           6: "ImageIdentificationAligned",
           7: "ImageIdentificationUpdateAligned",
           8: "ImageAlignment",
           9: "ProfilePictureUpdate"
        }
   },
   "RESPONSE": {  # response id range: integer
       "ID": {
           "Identification": 1,
           "Embedding": 2,
           "Image": 3,
           "QuadraticImage": 4,
           "Reidentification": 10,
           "OK": 111,
           "Error": 999
       },
       "NAME": {
           1: "Identification",
           2: "Embedding",
           3: "Image",
           4: "QuadraticImage",
           10: "Reidentification",
           111: "OK",
           999: "Error"
       }
   }
}
