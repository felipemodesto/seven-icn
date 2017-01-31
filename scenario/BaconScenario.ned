//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 
//package scenarios;

import org.car2x.veins.nodes.Scenario;
import paradise.bacon.*;

network BaconScenario extends Scenario
{
    parameters:
        //@display("bgb=341,200;bgi=background/terrain,s");
        double playgroundLatitude; // geographic position of the playground's north-west corner
        double playgroundLongitude; // geographic position of the playground's north-west corner
        double playgroundWidth @unit("m") = default(3000m);  // the E-W size of playground
        double playgroundHeight @unit("m") = default(3000m); // the N-S size of playground
        int numCows = default(0);  // the number of cows wandering around on the Boston Commons
        int numCars = default(0);  // the number of cars on the streets
        @display("bgb=3000,3000");
    submodules:
        statistics: BaconStatistics {
            parameters:
                @display("p=72,59;i=block/bucket");
        }

        library: BaconLibrary {
            parameters:
                @display("p=72,59;i=block/bucket");
        }

        //osgEarthScene: OsgEarthScene {
        //    @display("p=72,59");
        //}
}
