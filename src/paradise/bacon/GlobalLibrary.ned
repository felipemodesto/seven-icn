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

simple GlobalLibrary {
	parameters:
    	@class(GlobalLibrary);
		double zipfCaracterization = default(0);	   	//P Value Characterization for Zipf Function
		int sizeTransit = default(10240);				//Size of Items in Transit Object Library
		int sizeNetwork = default(10240);				//Size of Items in Network Related Library
		int sizeMultimedia = default(10240);			//Size of Items in Multimedia Library
		int libraryTransit = default(1000);				//Number of Items in Transit Object Library
		int libraryNetwork = default(1000);				//Number of Items in Network Related Library
		int libraryMultimedia = default(1000);			//Number of Items in Multimedia Library
		int priorityTransit = default(1);				//Priority of Items in Transit Object Library
		int priorityNetwork = default(2);				//Priority of Items in Network Related Library
		int priorityMultimedia = default(3);			//Priority of Items in Multimedia Library
		int locationCorrelationModel = default(0);		//Model for location correlation (default = 0 specifies no location correlation)
		
		int sectorSize = default(100);					//Width/Height of sector (square)
		int maximumViableDistance = default(200);		//Maximum euclidian distance between node and sector being requested in which node can viably have the content object (for GPS requests)
		
		int maxVehicleServers = default(0);				//Maximum number of concurrent vehicle servers in network
		int maxVehicleClients = default(-1);			//Number of vehicles not tagged as servers, which are tagged as clients (-1 for all)
		
        string requestSequenceFile = default("request-sequence.csv");
}
