/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#ifndef SHARED_HPP
#define SHARED_HPP

#include "config.h"

#ifdef HAVE_MEMORY
#include <memory>
#endif

#ifdef HAVE_TR1_MEMORY
#include <tr1/memory>
#endif

namespace shared {
#ifdef HAVE_STD_SHARED_PTR
  using std::shared_ptr;
  using std::make_shared;
#else
#ifdef HAVE_STD_TR1_SHARED_PTR
  using std::tr1::shared_ptr;

  // make_shared emulation.
  template<typename Tp, typename A1>
    inline shared_ptr<Tp>
    make_shared(const A1& a1)
  {
    return shared_ptr<Tp>(new Tp(a1));
  }
  template<typename Tp, typename A1, typename A2>
    inline shared_ptr<Tp>
    make_shared(const A1& a1, const A2& a2)
  {
    return shared_ptr<Tp>(new Tp(a1, a2));
  }
  template<typename Tp, typename A1, typename A2, typename A3>
    inline shared_ptr<Tp>
    make_shared(const A1& a1, const A2& a2, const A3& a3)
  {
    return shared_ptr<Tp>(new Tp(a1, a2, a3));
  }
  template<typename Tp, typename A1, typename A2, typename A3, typename A4>
    inline shared_ptr<Tp>
    make_shared(const A1& a1, const A2& a2, const A3& a3, const A4& a4)
  {
    return shared_ptr<Tp>(new Tp(a1, a2, a3, a4));
  }
#else
#error Need a shared_ptr class.  Try Boost::TR1.
#endif
#endif
}
#endif
