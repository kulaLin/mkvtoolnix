/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the packet structure

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_PACKET_H
#define MTX_PACKET_H

#include "common/common_pch.h"

#include "common/timestamp.h"

namespace libmatroska {
  class KaxBlock;
  class KaxBlockBlob;
  class KaxCluster;
};

using namespace libmatroska;

class generic_packetizer_c;
class track_statistics_c;

class packet_extension_c {
public:
  enum packet_extension_type_e {
    MULTIPLE_TIMECODES,
    SUBTITLE_NUMBER,
    BEFORE_ADDING_TO_CLUSTER_CB,
  };

public:
  packet_extension_c() {
  }

  virtual ~packet_extension_c() {
  }

  virtual packet_extension_type_e get_type() const = 0;
};
using packet_extension_cptr = std::shared_ptr<packet_extension_c>;

struct packet_t {
  memory_cptr data;
  std::vector<memory_cptr> data_adds;
  memory_cptr codec_state;

  KaxBlockBlob *group;
  KaxBlock *block;
  KaxCluster *cluster;
  int ref_priority, time_factor;
  int64_t timecode, bref, fref, duration, assigned_timecode;
  int64_t timecode_before_factory;
  int64_t unmodified_assigned_timecode, unmodified_duration;
  boost::optional<uint64_t> uncompressed_size;
  timestamp_c discard_padding, output_order_timecode;
  bool duration_mandatory, superseeded, gap_following, factory_applied;
  generic_packetizer_c *source;

  std::vector<packet_extension_cptr> extensions;

  packet_t()
    : group{}
    , block{}
    , cluster{}
    , ref_priority{}
    , time_factor(1)
    , timecode{}
    , bref{}
    , fref{}
    , duration(-1)
    , assigned_timecode{}
    , timecode_before_factory{}
    , unmodified_assigned_timecode{}
    , unmodified_duration{}
    , discard_padding{}
    , duration_mandatory{}
    , superseeded{}
    , gap_following{}
    , factory_applied{}
    , source{}
  {
  }

  packet_t(memory_cptr p_memory,
           int64_t p_timecode = -1,
           int64_t p_duration = -1,
           int64_t p_bref     = -1,
           int64_t p_fref     = -1)
    : data(p_memory)
    , group{}
    , block{}
    , cluster{}
    , ref_priority{}
    , time_factor(1)
    , timecode(p_timecode)
    , bref(p_bref)
    , fref(p_fref)
    , duration(p_duration)
    , assigned_timecode{}
    , timecode_before_factory{}
    , unmodified_assigned_timecode{}
    , unmodified_duration{}
    , discard_padding{}
    , duration_mandatory{}
    , superseeded{}
    , gap_following{}
    , factory_applied{}
    , source{}
  {
  }

  packet_t(memory_c *n_memory,
           int64_t p_timecode = -1,
           int64_t p_duration = -1,
           int64_t p_bref     = -1,
           int64_t p_fref     = -1)
    : data(memory_cptr(n_memory))
    , group{}
    , block{}
    , cluster{}
    , ref_priority{}
    , time_factor(1)
    , timecode(p_timecode)
    , bref(p_bref)
    , fref(p_fref)
    , duration(p_duration)
    , assigned_timecode{}
    , timecode_before_factory{}
    , unmodified_assigned_timecode{}
    , unmodified_duration{}
    , discard_padding{}
    , duration_mandatory{}
    , superseeded{}
    , gap_following{}
    , factory_applied{}
    , source{}
  {
  }

  ~packet_t() {
  }

  bool
  has_timecode()
    const {
    return 0 <= timecode;
  }

  bool
  has_bref()
    const {
    return 0 <= bref;
  }

  bool
  has_fref()
    const {
    return 0 <= fref;
  }

  bool
  has_duration()
    const {
    return 0 <= duration;
  }

  bool
  has_discard_padding()
    const {
    return discard_padding.valid();
  }

  int64_t
  get_duration()
    const {
    return has_duration() ? duration : 0;
  }

  int64_t
  get_unmodified_duration()
    const {
    return has_duration() ? unmodified_duration : 0;
  }

  bool
  is_key_frame()
    const {
    return !has_bref() && !has_fref();
  }

  bool
  is_p_frame()
    const {
    return (has_bref() || has_fref()) && (has_bref() != has_fref());
  }

  bool
  is_b_frame()
    const {
    return has_bref() && has_fref();
  }

  packet_extension_c *
  find_extension(packet_extension_c::packet_extension_type_e type)
    const {
    for (auto &extension : extensions)
      if (extension->get_type() == type)
        return extension.get();

    return nullptr;
  }

  void add_extensions(std::vector<packet_extension_cptr> const &new_extensions);

  void normalize_timecodes();

  void account(track_statistics_c &statistics, int64_t timestamp_offset) const;
  uint64_t calculate_uncompressed_size();
};
using packet_cptr = std::shared_ptr<packet_t>;

#endif // MTX_PACKET_H
