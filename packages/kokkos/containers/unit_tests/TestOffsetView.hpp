//@HEADER
// ************************************************************************
//
//                        Kokkos v. 4.0
//       Copyright (2022) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Part of Kokkos, under the Apache License v2.0 with LLVM Exceptions.
// See https://kokkos.org/LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//@HEADER

/*
 * FIXME the OffsetView class is really not very well tested.
 */
#ifndef CONTAINERS_UNIT_TESTS_TESTOFFSETVIEW_HPP_
#define CONTAINERS_UNIT_TESTS_TESTOFFSETVIEW_HPP_

#include <gtest/gtest.h>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <Kokkos_Timer.hpp>
#include <Kokkos_OffsetView.hpp>

using std::cout;
using std::endl;

namespace Test {

template <typename Scalar, typename Device>
void test_offsetview_construction() {
  using offset_view_type = Kokkos::Experimental::OffsetView<Scalar**, Device>;
  using view_type        = Kokkos::View<Scalar**, Device>;

  std::pair<int64_t, int64_t> range0 = {-1, 3};
  std::pair<int64_t, int64_t> range1 = {-2, 2};

  {
    offset_view_type o1;
    ASSERT_FALSE(o1.is_allocated());

    o1 = offset_view_type("o1", {-1, 3}, {-2, 2});
    offset_view_type o2(o1);
    offset_view_type o3("o3", range0, range1);

    ASSERT_TRUE(o1.is_allocated());
    ASSERT_TRUE(o2.is_allocated());
    ASSERT_TRUE(o3.is_allocated());
  }

  offset_view_type ov("firstOV", range0, range1);

  ASSERT_EQ("firstOV", ov.label());

#ifdef KOKKOS_ENABLE_DEPRECATION_WARNINGS
  KOKKOS_IMPL_DISABLE_DEPRECATED_WARNINGS_PUSH()
#endif
#ifdef KOKKOS_ENABLE_DEPRECATED_CODE_4
  ASSERT_EQ(2u, ov.Rank);
#endif
#ifdef KOKKOS_ENABLE_DEPRECATION_WARNINGS
  KOKKOS_IMPL_DISABLE_DEPRECATED_WARNINGS_POP()
#endif

  ASSERT_EQ(2u, ov.rank());

  ASSERT_EQ(ov.begin(0), -1);
  ASSERT_EQ(ov.end(0), 4);

  ASSERT_EQ(ov.begin(1), -2);
  ASSERT_EQ(ov.end(1), 3);

  ASSERT_EQ(ov.extent(0), 5u);
  ASSERT_EQ(ov.extent(1), 5u);

  {
    Kokkos::Experimental::OffsetView<Scalar*, Device> offsetV1("OneDOffsetView",
                                                               range0);

    Kokkos::RangePolicy<Device, int> rangePolicy1(offsetV1.begin(0),
                                                  offsetV1.end(0));
    Kokkos::parallel_for(
        rangePolicy1, KOKKOS_LAMBDA(const int i) { offsetV1(i) = 1; });
    Kokkos::fence();

    int OVResult = 0;
    Kokkos::parallel_reduce(
        rangePolicy1,
        KOKKOS_LAMBDA(const int i, int& updateMe) { updateMe += offsetV1(i); },
        OVResult);

    Kokkos::fence();
    ASSERT_EQ(OVResult, offsetV1.end(0) - offsetV1.begin(0))
        << "found wrong number of elements in OffsetView that was summed.";
  }
  {  // test deep copy of scalar const value into mirro
    const int constVal = 6;
    typename offset_view_type::HostMirror hostOffsetView =
        Kokkos::create_mirror_view(ov);

    Kokkos::deep_copy(hostOffsetView, constVal);

    for (int i = hostOffsetView.begin(0); i < hostOffsetView.end(0); ++i) {
      for (int j = hostOffsetView.begin(1); j < hostOffsetView.end(1); ++j) {
        ASSERT_EQ(hostOffsetView(i, j), constVal)
            << "Bad data found in OffsetView";
      }
    }
  }

  const int ovmin0 = ov.begin(0);
  const int ovend0 = ov.end(0);
  const int ovmin1 = ov.begin(1);
  const int ovend1 = ov.end(1);

  using range_type =
      Kokkos::MDRangePolicy<Device, Kokkos::Rank<2>, Kokkos::IndexType<int> >;
  using point_type = typename range_type::point_type;

  range_type rangePolicy2D(point_type{{ovmin0, ovmin1}},
                           point_type{{ovend0, ovend1}});

  const int constValue = 9;
  Kokkos::parallel_for(
      rangePolicy2D,
      KOKKOS_LAMBDA(const int i, const int j) { ov(i, j) = constValue; });

  // test offsetview to offsetviewmirror deep copy
  typename offset_view_type::HostMirror hostOffsetView =
      Kokkos::create_mirror_view(ov);

  Kokkos::deep_copy(hostOffsetView, ov);

  for (int i = hostOffsetView.begin(0); i < hostOffsetView.end(0); ++i) {
    for (int j = hostOffsetView.begin(1); j < hostOffsetView.end(1); ++j) {
      ASSERT_EQ(hostOffsetView(i, j), constValue)
          << "Bad data found in OffsetView";
    }
  }

  int OVResult = 0;
  Kokkos::parallel_reduce(
      rangePolicy2D,
      KOKKOS_LAMBDA(const int i, const int j, int& updateMe) {
        updateMe += ov(i, j);
      },
      OVResult);

  int answer = 0;
  for (int i = ov.begin(0); i < ov.end(0); ++i) {
    for (int j = ov.begin(1); j < ov.end(1); ++j) {
      answer += constValue;
    }
  }

  ASSERT_EQ(OVResult, answer) << "Bad data found in OffsetView";

  {
    offset_view_type ovCopy(ov);
    ASSERT_EQ(ovCopy == ov, true)
        << "Copy constructor or equivalence operator broken";
  }

  {
    offset_view_type ovAssigned = ov;
    ASSERT_EQ(ovAssigned == ov, true)
        << "Assignment operator or equivalence operator broken";
  }

  {  // construct OffsetView from a View plus begins array
    const int extent0 = 100;
    const int extent1 = 200;
    const int extent2 = 300;
    Kokkos::View<Scalar***, Device> view3D("view3D", extent0, extent1, extent2);

    Kokkos::deep_copy(view3D, 1);

    using range3_type = Kokkos::MDRangePolicy<Device, Kokkos::Rank<3>,
                                              Kokkos::IndexType<int64_t> >;
    using point3_type = typename range3_type::point_type;

    typename point3_type::value_type begins0 = -10, begins1 = -20,
                                     begins2 = -30;
    Kokkos::Array<int64_t, 3> begins         = {{begins0, begins1, begins2}};
    Kokkos::Experimental::OffsetView<Scalar***, Device> offsetView3D(view3D,
                                                                     begins);

    range3_type rangePolicy3DZero(point3_type{{0, 0, 0}},
                                  point3_type{{extent0, extent1, extent2}});

    int view3DSum = 0;
    Kokkos::parallel_reduce(
        rangePolicy3DZero,
        KOKKOS_LAMBDA(const int i, const int j, int k, int& updateMe) {
          updateMe += view3D(i, j, k);
        },
        view3DSum);

    range3_type rangePolicy3D(
        point3_type{{begins0, begins1, begins2}},
        point3_type{{begins0 + extent0, begins1 + extent1, begins2 + extent2}});
    int offsetView3DSum = 0;

    Kokkos::parallel_reduce(
        rangePolicy3D,
        KOKKOS_LAMBDA(const int i, const int j, int k, int& updateMe) {
          updateMe += offsetView3D(i, j, k);
        },
        offsetView3DSum);

    ASSERT_EQ(view3DSum, offsetView3DSum)
        << "construction of OffsetView from View and begins array broken.";
  }
  view_type viewFromOV = ov.view();

  ASSERT_EQ(viewFromOV == ov, true)
      << "OffsetView::view() or equivalence operator View == OffsetView broken";

  {
    offset_view_type ovFromV(viewFromOV, {-1, -2});

    ASSERT_EQ(ovFromV == viewFromOV, true)
        << "Construction of OffsetView from View or equivalence operator "
           "OffsetView == View broken";
  }
  {
    offset_view_type ovFromV = viewFromOV;
    ASSERT_EQ(ovFromV == viewFromOV, true)
        << "Construction of OffsetView from View by assignment (implicit "
           "conversion) or equivalence operator OffsetView == View broken";
  }

  {  // test offsetview to view deep copy
    view_type aView("aView", ov.extent(0), ov.extent(1));
    Kokkos::deep_copy(aView, ov);

    int sum = 0;
    Kokkos::parallel_reduce(
        rangePolicy2D,
        KOKKOS_LAMBDA(const int i, const int j, int& updateMe) {
          updateMe += ov(i, j) - aView(i - ov.begin(0), j - ov.begin(1));
        },
        sum);

    ASSERT_EQ(sum, 0) << "deep_copy(view, offsetView) broken.";
  }

  {  // test view to  offsetview deep copy
    view_type aView("aView", ov.extent(0), ov.extent(1));

    Kokkos::deep_copy(aView, 99);
    Kokkos::deep_copy(ov, aView);

    int sum = 0;
    Kokkos::parallel_reduce(
        rangePolicy2D,
        KOKKOS_LAMBDA(const int i, const int j, int& updateMe) {
          updateMe += ov(i, j) - aView(i - ov.begin(0), j - ov.begin(1));
        },
        sum);

    ASSERT_EQ(sum, 0) << "deep_copy(offsetView, view) broken.";
  }
}

template <typename Scalar, typename Device>
void test_offsetview_unmanaged_construction() {
  // Preallocated memory (Only need a valid address for this test)
  Scalar s;

  {
    // Constructing an OffsetView directly around our preallocated memory
    Kokkos::Array<int64_t, 1> begins1{{2}};
    Kokkos::Array<int64_t, 1> ends1{{3}};
    Kokkos::Experimental::OffsetView<Scalar*, Device> ov1(&s, begins1, ends1);

    // Constructing an OffsetView around an unmanaged View of our preallocated
    // memory
    Kokkos::View<Scalar*, Device> v1(&s, ends1[0] - begins1[0]);
    Kokkos::Experimental::OffsetView<Scalar*, Device> ovv1(v1, begins1);

    // They should match
    ASSERT_EQ(ovv1, ov1)
        << "OffsetView unmanaged construction fails for rank 1";
  }

  {
    Kokkos::Array<int64_t, 2> begins2{{-2, -7}};
    Kokkos::Array<int64_t, 2> ends2{{5, -3}};
    Kokkos::Experimental::OffsetView<Scalar**, Device> ov2(&s, begins2, ends2);

    Kokkos::View<Scalar**, Device> v2(&s, ends2[0] - begins2[0],
                                      ends2[1] - begins2[1]);
    Kokkos::Experimental::OffsetView<Scalar**, Device> ovv2(v2, begins2);

    ASSERT_EQ(ovv2, ov2)
        << "OffsetView unmanaged construction fails for rank 2";
  }

  {
    Kokkos::Array<int64_t, 3> begins3{{2, 3, 5}};
    Kokkos::Array<int64_t, 3> ends3{{7, 11, 13}};
    Kokkos::Experimental::OffsetView<Scalar***, Device> ovv3(&s, begins3,
                                                             ends3);

    Kokkos::View<Scalar***, Device> v3(&s, ends3[0] - begins3[0],
                                       ends3[1] - begins3[1],
                                       ends3[2] - begins3[2]);
    Kokkos::Experimental::OffsetView<Scalar***, Device> ov3(v3, begins3);

    ASSERT_EQ(ovv3, ov3)
        << "OffsetView unmanaged construction fails for rank 3";
  }

  {
    // Test all four public constructor overloads (begins_type x
    // index_list_type)
    Kokkos::Array<int64_t, 1> begins{{-3}};
    Kokkos::Array<int64_t, 1> ends{{2}};

    Kokkos::Experimental::OffsetView<Scalar*, Device> bb(&s, begins, ends);
    Kokkos::Experimental::OffsetView<Scalar*, Device> bi(&s, begins, {2});
    Kokkos::Experimental::OffsetView<Scalar*, Device> ib(&s, {-3}, ends);
    Kokkos::Experimental::OffsetView<Scalar*, Device> ii(&s, {-3}, {2});

    ASSERT_EQ(bb, bi);
    ASSERT_EQ(bb, ib);
    ASSERT_EQ(bb, ii);
  }
}

template <typename Scalar, typename Device>
void test_offsetview_unmanaged_construction_death() {
  // Preallocated memory (Only need a valid address for this test)
  Scalar s;

  // Regular expression syntax on Windows is a pain. `.` does not match `\n`.
  // Feel free to make it work if you have time to spare.
#ifdef _WIN32
#define SKIP_REGEX_ON_WINDOWS(REGEX) ""
#else
#define SKIP_REGEX_ON_WINDOWS(REGEX) REGEX
#endif

  {
    using offset_view_type = Kokkos::Experimental::OffsetView<Scalar*, Device>;

    // Range calculations must be positive
    (void)offset_view_type(&s, {0}, {1});
    (void)offset_view_type(&s, {0}, {0});
    ASSERT_DEATH(
        offset_view_type(&s, {0}, {-1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "\\(ends\\[0\\] \\(-1\\) - begins\\[0\\] \\(0\\)\\) must be "
            "non-negative"));
  }

  {
    using offset_view_type = Kokkos::Experimental::OffsetView<Scalar*, Device>;

    // Range calculations must not overflow
    (void)offset_view_type(&s, {0}, {0x7fffffffffffffffl});
    ASSERT_DEATH(
        offset_view_type(&s, {-1}, {0x7fffffffffffffffl}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "\\(ends\\[0\\] \\(9223372036854775807\\) - begins\\[0\\] "
            "\\(-1\\)\\) "
            "overflows"));
    ASSERT_DEATH(
        offset_view_type(&s, {-0x7fffffffffffffffl - 1}, {0x7fffffffffffffffl}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "\\(ends\\[0\\] \\(9223372036854775807\\) - begins\\[0\\] "
            "\\(-9223372036854775808\\)\\) "
            "overflows"));
    ASSERT_DEATH(
        offset_view_type(&s, {-0x7fffffffffffffffl - 1}, {0}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "\\(ends\\[0\\] \\(0\\) - begins\\[0\\] "
            "\\(-9223372036854775808\\)\\) "
            "overflows"));
  }

  {
    using offset_view_type = Kokkos::Experimental::OffsetView<Scalar**, Device>;

    // Should throw when the rank of begins and/or ends doesn't match that
    // of OffsetView
    ASSERT_DEATH(
        offset_view_type(&s, {0}, {1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "begins\\.size\\(\\) \\(1\\) != Rank \\(2\\)"
            ".*"
            "ends\\.size\\(\\) \\(1\\) != Rank \\(2\\)"));
    ASSERT_DEATH(
        offset_view_type(&s, {0}, {1, 1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "begins\\.size\\(\\) \\(1\\) != Rank \\(2\\)"));
    ASSERT_DEATH(
        offset_view_type(&s, {0}, {1, 1, 1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "begins\\.size\\(\\) \\(1\\) != Rank \\(2\\)"
            ".*"
            "ends\\.size\\(\\) \\(3\\) != Rank \\(2\\)"));
    ASSERT_DEATH(
        offset_view_type(&s, {0, 0}, {1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "ends\\.size\\(\\) \\(1\\) != Rank \\(2\\)"));
    (void)offset_view_type(&s, {0, 0}, {1, 1});
    ASSERT_DEATH(
        offset_view_type(&s, {0, 0}, {1, 1, 1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "ends\\.size\\(\\) \\(3\\) != Rank \\(2\\)"));
    ASSERT_DEATH(
        offset_view_type(&s, {0, 0, 0}, {1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "begins\\.size\\(\\) \\(3\\) != Rank \\(2\\)"
            ".*"
            "ends\\.size\\(\\) \\(1\\) != Rank \\(2\\)"));
    ASSERT_DEATH(
        offset_view_type(&s, {0, 0, 0}, {1, 1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "begins\\.size\\(\\) \\(3\\) != Rank \\(2\\)"));
    ASSERT_DEATH(
        offset_view_type(&s, {0, 0, 0}, {1, 1, 1}),
        SKIP_REGEX_ON_WINDOWS(
            "Kokkos::Experimental::OffsetView ERROR: for unmanaged OffsetView"
            ".*"
            "begins\\.size\\(\\) \\(3\\) != Rank \\(2\\)"
            ".*"
            "ends\\.size\\(\\) \\(3\\) != Rank \\(2\\)"));
  }
#undef SKIP_REGEX_ON_WINDOWS
}

template <typename Scalar, typename Device>
void test_offsetview_subview() {
  {  // test subview 1
    Kokkos::Experimental::OffsetView<Scalar*, Device> sliceMe("offsetToSlice",
                                                              {-10, 20});
    {
      auto offsetSubview = Kokkos::Experimental::subview(sliceMe, 0);
      ASSERT_EQ(offsetSubview.rank(), 0u) << "subview of offset is broken.";
    }
  }
  {  // test subview 2
    Kokkos::Experimental::OffsetView<Scalar**, Device> sliceMe(
        "offsetToSlice", {-10, 20}, {-20, 30});
    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, Kokkos::ALL(), -2);
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }

    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, 0, Kokkos::ALL());
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }
  }

  {  // test subview rank 3

    Kokkos::Experimental::OffsetView<Scalar***, Device> sliceMe(
        "offsetToSlice", {-10, 20}, {-20, 30}, {-30, 40});

    // slice 1
    {
      auto offsetSubview = Kokkos::Experimental::subview(sliceMe, Kokkos::ALL(),
                                                         Kokkos::ALL(), 0);
      ASSERT_EQ(offsetSubview.rank(), 2u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview = Kokkos::Experimental::subview(sliceMe, Kokkos::ALL(),
                                                         0, Kokkos::ALL());
      ASSERT_EQ(offsetSubview.rank(), 2u) << "subview of offset is broken.";
    }

    {
      auto offsetSubview = Kokkos::Experimental::subview(
          sliceMe, 0, Kokkos::ALL(), Kokkos::ALL());
      ASSERT_EQ(offsetSubview.rank(), 2u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview = Kokkos::Experimental::subview(
          sliceMe, 0, Kokkos::ALL(), Kokkos::make_pair(-30, -21));
      ASSERT_EQ(offsetSubview.rank(), 2u) << "subview of offset is broken.";

      ASSERT_EQ(offsetSubview.begin(0), -20);
      ASSERT_EQ(offsetSubview.end(0), 31);
      ASSERT_EQ(offsetSubview.begin(1), 0);
      ASSERT_EQ(offsetSubview.end(1), 9);

      using range_type = Kokkos::MDRangePolicy<Device, Kokkos::Rank<2>,
                                               Kokkos::IndexType<int> >;
      using point_type = typename range_type::point_type;

      const int b0 = offsetSubview.begin(0);
      const int b1 = offsetSubview.begin(1);

      const int e0 = offsetSubview.end(0);
      const int e1 = offsetSubview.end(1);

      range_type rangeP2D(point_type{{b0, b1}}, point_type{{e0, e1}});

      Kokkos::parallel_for(
          rangeP2D,
          KOKKOS_LAMBDA(const int i, const int j) { offsetSubview(i, j) = 6; });

      int sum = 0;
      Kokkos::parallel_reduce(
          rangeP2D,
          KOKKOS_LAMBDA(const int i, const int j, int& updateMe) {
            updateMe += offsetSubview(i, j);
          },
          sum);

      ASSERT_EQ(sum, 6 * (e0 - b0) * (e1 - b1));
    }

    // slice 2
    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, Kokkos::ALL(), 0, 0);
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, 0, 0, Kokkos::ALL());
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }

    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, 0, Kokkos::ALL(), 0);
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }
  }

  {  // test subview rank 4

    Kokkos::Experimental::OffsetView<Scalar****, Device> sliceMe(
        "offsetToSlice", {-10, 20}, {-20, 30}, {-30, 40}, {-40, 50});

    // slice 1
    {
      auto offsetSubview = Kokkos::Experimental::subview(
          sliceMe, Kokkos::ALL(), Kokkos::ALL(), Kokkos::ALL(), 0);
      ASSERT_EQ(offsetSubview.rank(), 3u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview = Kokkos::Experimental::subview(
          sliceMe, Kokkos::ALL(), Kokkos::ALL(), 0, Kokkos::ALL());
      ASSERT_EQ(offsetSubview.rank(), 3u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview = Kokkos::Experimental::subview(
          sliceMe, Kokkos::ALL(), 0, Kokkos::ALL(), Kokkos::ALL());
      ASSERT_EQ(offsetSubview.rank(), 3u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview = Kokkos::Experimental::subview(
          sliceMe, 0, Kokkos::ALL(), Kokkos::ALL(), Kokkos::ALL());
      ASSERT_EQ(offsetSubview.rank(), 3u) << "subview of offset is broken.";
    }

    // slice 2
    auto offsetSubview2a = Kokkos::Experimental::subview(sliceMe, Kokkos::ALL(),
                                                         Kokkos::ALL(), 0, 0);
    ASSERT_EQ(offsetSubview2a.rank(), 2u) << "subview of offset is broken.";
    {
      auto offsetSubview2b = Kokkos::Experimental::subview(
          sliceMe, Kokkos::ALL(), 0, Kokkos::ALL(), 0);
      ASSERT_EQ(offsetSubview2b.rank(), 2u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview2b = Kokkos::Experimental::subview(
          sliceMe, Kokkos::ALL(), 0, 0, Kokkos::ALL());
      ASSERT_EQ(offsetSubview2b.rank(), 2u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview2b = Kokkos::Experimental::subview(
          sliceMe, 0, Kokkos::ALL(), 0, Kokkos::ALL());
      ASSERT_EQ(offsetSubview2b.rank(), 2u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview2b = Kokkos::Experimental::subview(
          sliceMe, 0, 0, Kokkos::ALL(), Kokkos::ALL());
      ASSERT_EQ(offsetSubview2b.rank(), 2u) << "subview of offset is broken.";
    }
    // slice 3
    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, Kokkos::ALL(), 0, 0, 0);
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, 0, Kokkos::ALL(), 0, 0);
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, 0, 0, Kokkos::ALL(), 0);
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }
    {
      auto offsetSubview =
          Kokkos::Experimental::subview(sliceMe, 0, 0, 0, Kokkos::ALL());
      ASSERT_EQ(offsetSubview.rank(), 1u) << "subview of offset is broken.";
    }
  }
}

template <class InputIt, class T, class BinaryOperation>
KOKKOS_INLINE_FUNCTION T std_accumulate(InputIt first, InputIt last, T init,
                                        BinaryOperation op) {
  for (; first != last; ++first) {
    init = op(std::move(init), *first);
  }
  return init;
}

KOKKOS_INLINE_FUNCTION int element(std::initializer_list<int> il) {
  return std_accumulate(il.begin(), il.end(), 0,
                        [](int l, int r) { return l * 10 + r; });
}

template <typename DEVICE>
void test_offsetview_offsets_rank1() {
  using data_type        = int*;
  using view_type        = Kokkos::View<data_type, DEVICE>;
  using index_type       = Kokkos::IndexType<int>;
  using execution_policy = Kokkos::RangePolicy<DEVICE, index_type>;
  using offset_view_type = Kokkos::Experimental::OffsetView<data_type, DEVICE>;

  view_type v("View1", 10);
  Kokkos::parallel_for(
      "For1", execution_policy(0, v.extent_int(0)),
      KOKKOS_LAMBDA(const int i) { v(i) = element({i}); });

  int errors;
  Kokkos::parallel_reduce(
      "Reduce1", execution_policy(-3, 4),
      KOKKOS_LAMBDA(const int ii, int& lerrors) {
        offset_view_type ov(v, {ii});
        lerrors += (ov(3) != element({3 - ii}));
        lerrors += (ov[3] != element({3 - ii}));
      },
      errors);

  ASSERT_EQ(0, errors);
}

template <typename DEVICE>
void test_offsetview_offsets_rank2() {
  using data_type        = int**;
  using view_type        = Kokkos::View<data_type, DEVICE>;
  using index_type       = Kokkos::IndexType<int>;
  using execution_policy = Kokkos::RangePolicy<DEVICE, index_type>;
  using offset_view_type = Kokkos::Experimental::OffsetView<data_type, DEVICE>;

  view_type v("View2", 10, 10);
  Kokkos::parallel_for(
      "For2", execution_policy(0, v.extent_int(0)), KOKKOS_LAMBDA(const int i) {
        for (int j = 0; j != v.extent_int(1); ++j) {
          v(i, j) = element({i, j});
        }
      });

  int errors;
  Kokkos::parallel_reduce(
      "Reduce2", execution_policy(-3, 4),
      KOKKOS_LAMBDA(const int ii, int& lerrors) {
        for (int jj = -3; jj <= 3; ++jj) {
          offset_view_type ov(v, {ii, jj});
          lerrors += (ov(3, 3) != element({3 - ii, 3 - jj}));
        }
      },
      errors);

  ASSERT_EQ(0, errors);
}

template <typename DEVICE>
void test_offsetview_offsets_rank3() {
  using data_type        = int***;
  using view_type        = Kokkos::View<data_type, DEVICE>;
  using index_type       = Kokkos::IndexType<int>;
  using execution_policy = Kokkos::RangePolicy<DEVICE, index_type>;
  using offset_view_type = Kokkos::Experimental::OffsetView<data_type, DEVICE>;

  view_type v("View3", 10, 10, 10);
  Kokkos::parallel_for(
      "For3", execution_policy(0, v.extent_int(0)), KOKKOS_LAMBDA(const int i) {
        for (int j = 0; j != v.extent_int(1); ++j) {
          for (int k = 0; k != v.extent_int(2); ++k) {
            v(i, j, k) = element({i, j, k});
          }
        }
      });

  int errors;
  Kokkos::parallel_reduce(
      "Reduce3", execution_policy(-3, 4),
      KOKKOS_LAMBDA(const int ii, int& lerrors) {
        for (int jj = -3; jj <= 3; ++jj) {
          for (int kk = -3; kk <= 3; ++kk) {
            offset_view_type ov(v, {ii, jj, kk});
            lerrors += (ov(3, 3, 3) != element({3 - ii, 3 - jj, 3 - kk}));
          }
        }
      },
      errors);

  ASSERT_EQ(0, errors);
}

TEST(TEST_CATEGORY, offsetview_construction) {
  test_offsetview_construction<int, TEST_EXECSPACE>();
}

TEST(TEST_CATEGORY, offsetview_unmanaged_construction) {
  test_offsetview_unmanaged_construction<int, TEST_EXECSPACE>();
}

TEST(TEST_CATEGORY_DEATH, offsetview_unmanaged_construction) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  test_offsetview_unmanaged_construction_death<int, TEST_EXECSPACE>();
}

TEST(TEST_CATEGORY, offsetview_subview) {
  test_offsetview_subview<int, TEST_EXECSPACE>();
}

TEST(TEST_CATEGORY, offsetview_offsets_rank1) {
  test_offsetview_offsets_rank1<TEST_EXECSPACE>();
}

TEST(TEST_CATEGORY, offsetview_offsets_rank2) {
  test_offsetview_offsets_rank2<TEST_EXECSPACE>();
}

TEST(TEST_CATEGORY, offsetview_offsets_rank3) {
  test_offsetview_offsets_rank3<TEST_EXECSPACE>();
}

}  // namespace Test

#endif /* CONTAINERS_UNIT_TESTS_TESTOFFSETVIEW_HPP_ */
