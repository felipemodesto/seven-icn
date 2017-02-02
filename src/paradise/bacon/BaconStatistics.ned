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

simple BaconStatistics {
	parameters:
    	@class(BaconStatistics);
        bool collectingStatistics = default(true);
        bool collectingPositions = default(false);
        bool collectingRequestNames = default(false);
		double statisticsStartTime  = default(0); 		//First Time Stamp before statistics data is collected
		double statisticsStopTime  = default(100); 		//Final moment until which statistics are collected
		string locationStatisticsFile  = default("heatmap.csv");
		string contentNameStatisticsFile  = default("library.csv");
}
