//
// Copyright (C) 2006-2011 Christoph Sommer <christoph.sommer@uibk.ac.at>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

package paradise.bacon;

import org.car2x.veins.base.modules.IBaseApplLayer;
import org.car2x.veins.modules.application.ieee80211p.BaseWaveApplLayer;

simple BaconClient extends BaseWaveApplLayer {
    parameters:
    	@class(BaconClient);
		double minimumRequestDelay  = default(0); 				//Minimum time in between requests
		double maximumRequestDelay  = default(0);				//Maximum time in between requests
		double requestTimeout = default(2s) @unit(s);			//Time limit after which request is considered for "no response"
		double locationTimerDelay  = default(1s) @unit(s);		//Maximum time until Content Request
		
		int maxOpenRequests = default(20);				//Maximum number of unanswered requests a node can have at the same time
		
		double multimediaInterest = default(1);			//Interest in Multimedia Content
		double trafficInterest = default(1);			//Interest in Road Traffic Content
		double networkInterest = default(1);			//Interest in Network Content
		double emergencyInterest = default(0);			//Interest in Emergency Content
		
		//string modelURL;                             // the URL/filename of the 3D model to be used for the node (you can use osg pseudo filters like: modelname.osgb.3.scale.0,0,45.rot scaleX3, rotate 45 deg around Z)
        //string modelColor = default("");             // colorizes the 3D model used for the node decorations and annotations
        //string labelColor = default("#000000ff");    // the color of the model label in hex RRGGBBAA format or "" to turn off labels
        //double txRange @unit("m") = default(200m);   // the transmission range of the mobile node's radio
        //bool showTxRange = default(true);            // whether to show the transmission range around the nodes
        //string rangeColor = default("#ff000040");    // the color of the range indicator in hex RRGGBBAA format
        //string trailColor = default("#000000ff");    // the color of the trail indicator in hex RRGGBBAA format
        //int trailLength = default(0);	             // the length of the trail in time steps showing the past movement of the node. 0 disables the trail

    gates:   
        input clientExchangeIn; 		// from Service Manager
        output clientExchangeOut;		// to Service Manager
}
