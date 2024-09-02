/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2020  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#ifndef ORCHID_FAMILY_HPP
#define ORCHID_FAMILY_HPP

#include "link.hpp"

namespace orc {

class Family :
    public Link<Buffer>,
    public Sunken<Pump<Buffer>>
{
  private:
    uint32_t Analyze(const Window &data) {
        uint8_t vhl = 0;
        data.snip(1).copy(&vhl, 1);
        auto protocol(vhl >> 4);
        switch (protocol) {
        case 4: return AF_INET;
        case 6: return AF_INET6;
        default: return 0;
        }
    }

  protected:
    void Land(const Buffer &data) override {
        const auto [protocol, packet] = Take<Number<uint32_t>, Window>(data);
        orc_assert(protocol == Analyze(packet));
        return Link::Land(packet);
    }

  public:
    Family(BufferDrain &drain) :
        Link<Buffer>(typeid(*this).name(), drain)
    {
    }

    task<void> Shut() noexcept override {
        co_await Sunken::Shut();
        co_await Link::Shut();
    }

    task<void> Send(const Buffer &data) override {
        co_return co_await Inner().Send(Tie(Number<uint32_t>(Analyze(data)), data));
    }
};

}

#endif//ORCHID_FAMILY_HPP
