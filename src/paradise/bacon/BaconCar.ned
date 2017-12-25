//
// Copyright (C) 2008 Christoph Sommer <christoph.sommer@informatik.uni-erlangen.de>
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

import paradise.bacon.*;
import org.car2x.veins.base.modules.*;
import org.car2x.veins.modules.nic.Nic80211p;

module BaconCar
//extends MobileNode
{
    parameters:
        string nicType = default("Nic80211p"); // type of network interface card
        string veinsmobilityType; //type of the mobility module

    gates:
        input veinsradioIn; // gate for sendDirect

    submodules:

        client: BaconClient {
            parameters:
                @display("i=device/wifilaptop;p=60,79");
        }

        appl: BaconServiceManager {
            parameters:
                @display("i=device/device;p=60,167");
        }

        content: BaconContentProvider {
            parameters:
                @display("i=device/disk;p=216,167");
        }

        nic: Nic80211p {
            parameters:
                @display("i=device/accesspoint;p=60,276");
        }

        veinsmobility: <veinsmobilityType> like org.car2x.veins.base.modules.IMobility {
            parameters:
                @display("i=block/cogwheel;p=216,276");
        }

    connections:
        nic.upperLayerOut --> appl.lowerLayerIn;
        nic.upperLayerIn <-- appl.lowerLayerOut;
        nic.upperControlOut --> appl.lowerControlIn;
        nic.upperControlIn <-- appl.lowerControlOut;

        appl.clientExchangeIn <-- client.clientExchangeOut;
        appl.clientExchangeOut --> client.clientExchangeIn;
        client.lowerControlOut --> client.lowerControlIn;
        client.lowerLayerOut --> client.lowerLayerIn;
        //appl.contentExchangeIn <-- content.contentExchangeOut;
        //appl.contentExchangeOut --> content.contentExchangeIn;
        //content.lowerControlOut --> content.lowerControlIn;
        //content.lowerLayerOut --> content.lowerLayerIn;

        veinsradioIn --> nic.radioIn;
}
