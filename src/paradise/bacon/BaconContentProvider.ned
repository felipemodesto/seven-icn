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

simple BaconContentProvider {
    parameters:
    	@class(BaconContentProvider);

		int cacheReplacementPolicy = default(0);					//Max Replacement Policy : Enum = { RANDOM = 0, LRU = 1, FIFO = 2, BIG = 3, MULT_FIRST = 4, GPS_FIRST = 5 }
		int availableMultimediaObjects = default(50);				//Initial number of content items available in cache
		int maxCachedContents = default(100);						//Maximum Cache Size
		
    //gates:
        //input contentExchangeIn;		// from Service Manager
        //output contentExchangeOut;		// to Service Manager
}
