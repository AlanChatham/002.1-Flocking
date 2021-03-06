002.1s

This series, 002, 002.1, and 002.2, are based off of the works of 
Robert Hodgin's visual projects made for the Eyeo2012 conference. 
I would have forked them cleanly, but that repo is all one big chunk,
so I wanted to break out the projects separately.
You can find his original code here:
https://github.com/flight404/Eyeo2012

So, what's different for these projects from his code?
I've implemented 3-d head tracking within Cinder, and so the Kinect code
here is sending out the position of the viewer's head, and the camera 
viewpoint gets distorted appropriately from that information. 
Specifically, we're using two cameras, then projecting the images
onto two screens positioned at a 90 degree angle, making for a virtual 
aquarium/cube viewer, giving the tracked viewer the impression that the 
screens are actually the exterior faces of a box in which the visuals 
are running within.

It assumes two projectors, projecting images on screens that are
put at 90 degrees to each other. The spacing of the Kinects is 
also hard-coded for now, although it should be compatible with 
the latest of our versions of Kinect-OSC, which provides for 
dynamic position tweaking of the sensors, but that will be 
a little less 'out-of-the-box' than this.

In order to compile this, you'll need to download Cinder
http://libcinder.org/releases/cinder_0.8.5_vc2012.zip
and unzip that into the 'PutCinderHere' folder

You'll also need to be running Visual Studio 2012
(NOT VS 2013!!!), but the Express (free) version is fine,
so I'd go download that if I were you.

As is common with my projects, all this code is released under the 
GNU Public License, v3.

Robert Hodgin's original work is distributed pretty much license-free 
on the above link, so if you just want the visuals, go there! 
The head tracking stuff is the main part I care about sharing 
under the GPL, so, yeah. Enjoy!