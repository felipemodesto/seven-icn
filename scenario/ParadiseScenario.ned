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

//import org.car2x.veins.nodes.Scenario;

import org.car2x.veins.base.connectionManager.ConnectionManager;
import org.car2x.veins.base.modules.BaseWorldUtility;
import org.car2x.veins.modules.mobility.traci.TraCIScenarioManagerLaunchd;
import org.car2x.veins.modules.obstacle.ObstacleControl;
import org.car2x.veins.modules.world.annotations.AnnotationManager;
import paradise.bacon.*;

network ParadiseScenario {
    parameters:
        //@display("bgb=341,200;bgi=background/terrain,s");
        
        double playgroundSizeX @unit(m); // x size of the area the nodes are in (in meters)
        double playgroundSizeY @unit(m); // y size of the area the nodes are in (in meters)
        double playgroundSizeZ @unit(m); // z size of the area the nodes are in (in meters)
        @display("bgb=3000,3000");
        
        double playgroundLatitude; // geographic position of the playground's north-west corner
        double playgroundLongitude; // geographic position of the playground's north-west corner
        double playgroundWidth @unit("m") = default(10000m);  // the E-W size of playground
        double playgroundHeight @unit("m") = default(10000m); // the N-S size of playground
        int numCows = default(0);  // the number of cows wandering around on the Boston Commons
        int numCars = default(0);  // the number of cars on the streets

        //@display("bgb=10000,10000");
    submodules:
        //obstacles: ObstacleControl {
        //    @display("p=72,59");
        //}
        
        annotations: AnnotationManager {
            @display("p=961.28,54.072002");
        }
        connectionManager: ConnectionManager {
            parameters:
                @display("p=66.088005,889.184;i=abstract/multicast");
        }
        world: BaseWorldUtility {
            parameters:
                playgroundSizeX = playgroundSizeX;
                playgroundSizeY = playgroundSizeY;
                playgroundSizeZ = playgroundSizeZ;
                @display("p=66.088005,474.63202;i=misc/globe");
        }
        manager: TraCIScenarioManagerLaunchd {
            parameters:
                @display("p=492.656,54.072002");
        }
        
        statistics: Statistics {
            parameters:
                @display("p=66.088005,1694.2561;i=block/bucket");
        }

        library: GlobalLibrary {
            parameters:
                @display("p=66.088005,1297.728;i=block/bucket");
        }

        //osgEarthScene: OsgEarthScene {
        //    @display("p=72,59");
        //}
        
    	connections allowunconnected:
}
