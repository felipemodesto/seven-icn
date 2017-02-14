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

simple BaconServiceManager extends BaseWaveApplLayer {
    parameters:
    	@class(BaconServiceManager);
    	@display("i=block/routing;p=250,58");
    	bool sendWhileParking  = default(true); 			//send messages when the car is parked
    	double maxSimultaneousConnections = default(-1);	//Number of simultaneous connected allowed for a node (-1 == Infinite)
		double transferTimeoutTime = default(10);			//Time before transfer timer is timed out
    	int inNetworkCaching  = default(0);		 			//in-network caching policy
    	double cacheCopyProbability  = default(0.5);		//in-network probabilistic cache replacement probability
    	double interestBroadcastTimeout = default(2);		//Time before an interest broadcast is timed out
    	int maxAttempts = default(3);						//Maximum number of attempts with failure before we give up on a connection
        double windowSlotSize = default(1s) @unit(s); 		//Duration of a time slot
		double minimumForwardDelay = default(0s) @unit(s); 	//Minimum Request Forward Delay
		double maximumForwardDelay = default(0s) @unit(s); 	//Maximum Request Forward Delay
        double bitrate @unit(bps) = default(18 Mbps);		//NOTE: This has to match the value in the MAC1609_4 module or things will break
    	
    	int slidingWindowSize = default (1);				//Sliding Window for Data Rate (seconds)
    	
        @signal[parkingStateChanged](type="BaseMobility");
    gates:       
        input clientExchangeIn; 		// from Application Layer
        output clientExchangeOut;		// to Application Layer
}
