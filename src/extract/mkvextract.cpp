/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   command line parsing, setup

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include <boost/regex.hpp>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "chapters.h"
#include "common.h"
#include "matroska.h"
#include "mkvextract.h"
#include "mm_io.h"
#include "tagwriter.h"
#include "xml_element_mapping.h"

using namespace libmatroska;
using namespace std;

#define NAME "mkvextract"

enum operation_mode_e {
  MODE_TRACKS,
  MODE_TAGS,
  MODE_ATTACHMENTS,
  MODE_CHAPTERS,
  MODE_CUESHEET,
  MODE_TIMECODES_V2,
};

bool g_no_variable_data = false;

void
set_usage() {
  usage_text = Y(
"Usage: mkvextract tracks <inname> [options] [TID1:out1 [TID2:out2 ...]]\n"
"   or  mkvextract tags <inname> [options]\n"
"   or  mkvextract attachments <inname> [options] [AID1:out1 [AID2:out2 ...]]"
"\n   or  mkvextract chapters <inname> [options]\n"
"   or  mkvextract cuesheet <inname> [options]\n"
"   or  mkvextract timecodes_v2 <inname> [TID1:out1 [TID2:out2 ...]]\n"
"   or  mkvextract <-h|-V>\n"
"\n"
" The first word tells mkvextract what to extract. The second must be the\n"
" source file. The only 'global' option that can be used with all modes is\n"
" '-v' or '--verbose' to increase the verbosity. All other options depend\n"
" on the mode.\n"
"\n"
" The first mode extracts some tracks to external files.\n"
"  -c charset       Convert text subtitles to this charset (default: UTF-8).\n"
"  --no-ogg         Write raw FLAC files (default: write OggFLAC files).\n"
"  --cuesheet       Also try to extract the CUE sheet from the chapter\n"
"                   information and tags for this track.\n"
"  --blockadd level Keep only the BlockAdditions up to this level\n"
"                   (default: keep all levels)\n"
"  --raw            Extract the data to a raw file.\n"
"  --fullraw        Extract the data to a raw file including the CodecPrivate"
"\n                   as a header.\n"
"  TID:out          Write track with the ID TID to the file 'out'.\n"
"\n"
" Example:\n"
" mkvextract tracks \"a movie.mkv\" 2:audio.ogg -c ISO8859-1 3:subs.srt\n"
"\n"
" The second mode extracts the tags and converts them to XML. The output is\n"
" written to the standard output. The output can be used as a source\n"
" for mkvmerge.\n"
"\n"
" Example:\n"
" mkvextract tags \"a movie.mkv\" > movie_tags.xml\n"
"\n"
" The third mode extracts attachments from the source file.\n"
"  AID:outname    Write the attachment with the ID 'AID' to 'outname'.\n"
"\n"
" Example:\n"
" mkvextract attachments \"a movie.mkv\" 4:cover.jpg\n"
"\n"
" The fourth mode extracts the chapters and converts them to XML. The\n"
" output is written to the standard output. The output can be used as a\n"
" source for mkvmerge.\n"
"  -s, --simple   Exports the chapter infomartion in the simple format\n"
"                 used in OGM tools (CHAPTER01=... CHAPTER01NAME=...).\n"
"\n"
" Example:\n"
" mkvextract chapters \"a movie.mkv\" > movie_chapters.xml\n"
"\n"
" The fifth mode tries to extract chapter information and tags and outputs\n"
" them as a CUE sheet. This is the reverse of using a CUE sheet with\n"
" mkvmerge's '--chapters' option.\n"
"\n"
" Example:\n"
" mkvextract cuesheet \"audiofile.mka\" > audiofile.cue\n"
"\n"
" The sixth mode finds the timecodes of all blocks for a track and outpus\n"
" a timecode v2 file with these timecodes.\n"
"\n"
" Example:\n"
" mkvextract timecodes_v2 \"a movie.mkv\" 1:timecodes_track1.txt\n"
"\n"
" These options can be used instead of the mode keyword to obtain\n"
" further information:\n"
"  -v, --verbose  Increase verbosity.\n"
"  -h, --help     Show this help.\n"
"  -V, --version  Show version information.\n");

  version_info = "mkvextract v" VERSION " ('" VERSIONNAME "')";
}

static bool s_chapter_format_simple = false;
static bool s_parse_fully           = false;

void
parse_args(vector<string> args,
           string &file_name,
           operation_mode_e &mode,
           vector<track_spec_t> &tracks) {

  verbose = 0;

  handle_common_cli_args(args, "-o");

  if (args.empty())
    usage();

  if      (args[0] == "tracks")
    mode = MODE_TRACKS;
  else if (args[0] == "tags")
    mode = MODE_TAGS;
  else if (args[0] == "attachments")
    mode = MODE_ATTACHMENTS;
  else if (args[0] == "chapters")
    mode = MODE_CHAPTERS;
  else if (args[0] == "cuesheet")
    mode = MODE_CUESHEET;
  else if (args[0] == "timecodes_v2")
    mode = MODE_TIMECODES_V2;
  else
    mxerror(boost::format(Y("Unknown mode '%1%'.\n")) % args[0]);

  if (args.size() < 2) {
    usage();
    mxexit(0);
  }

  file_name                  = args[1];

  const char *sub_charset    = "UTF-8";
  bool embed_in_ogg          = true;
  bool extract_cuesheet      = false;
  int extract_raw            = 0;
  int extract_blockadd_level = -1;

  boost::regex tid_re("^(\\d+)(:(.+))?$", boost::regex::perl);

  int i;
  // Now process all the other options.
  for (i = 2; i < args.size(); i++) {
    bool next_arg_available = args.size() > (i + 1);

    if ((args[i] == "--no-variable-data"))
      g_no_variable_data = true;

    else if ((args[i] == "-f") || (args[i] == "--parse-fully"))
      s_parse_fully = true;

    else if ((args[i] == "-c")) {
      if (MODE_TRACKS != mode)
        mxerror(Y("'-c' is only allowed when extracting tracks.\n"));

      if (!next_arg_available)
        mxerror(Y("'-c' lacks a charset.\n"));

      sub_charset = args[i + 1].c_str();
      i++;

    } else if ((args[i] == "--no-ogg")) {
      if (MODE_TRACKS != mode)
        mxerror(Y("'--no-ogg' is only allowed when extracting tracks.\n"));
      embed_in_ogg = false;

    } else if ((args[i] == "--cuesheet")) {
      if (MODE_TRACKS != mode)
        mxerror(Y("'--cuesheet' is only allowed when extracting tracks.\n"));
      extract_cuesheet = true;

    } else if ((args[i] == "--blockadd")) {
      if (MODE_TRACKS != mode)
        mxerror(Y("'--blockadd' is only allowed when extracting tracks.\n"));

      if (!next_arg_available)
        mxerror(Y("'--blockadd' lacks a level.\n"));

      if (!parse_int(args[i + 1], extract_blockadd_level) || (-1 > extract_blockadd_level))
        mxerror(boost::format(Y("Invalid BlockAddition level in argument '%1%'.\n")) % args[i + 1]);
      i++;

    } else if ((args[i] == "--raw")) {
      if (mode != MODE_TRACKS)
        mxerror(Y("'--raw' is only allowed when extracting tracks.\n"));
      extract_raw = 1;

    } else if ((args[i] == "--fullraw")) {
      if (mode != MODE_TRACKS)
        mxerror(Y("'--fullraw' is only allowed when extracting tracks.\n"));
      extract_raw = 2;

   } else if (mode == MODE_TAGS)
      mxerror(boost::format(Y("No further options allowed when extracting %1%.\n")) % args[0]);

    else if (mode == MODE_CUESHEET)
      mxerror(Y("No further options allowed when regenerating the CUE sheet.\n"));

    else if ((args[i] == "-s") || (args[i] == "--simple")) {
      if (MODE_CHAPTERS != mode)
        mxerror(boost::format(Y("'%1%' is only allowed for chapter extraction.\n")) % args[i]);

      s_chapter_format_simple = true;

    } else if ((mode == MODE_TRACKS) || (mode == MODE_ATTACHMENTS) || (MODE_TIMECODES_V2 == mode)) {
      boost::match_results<string::const_iterator> matches;
      if (!boost::regex_search(args[i], matches, tid_re))
        if ((MODE_TRACKS == mode) || (MODE_TIMECODES_V2 == mode))
          mxerror(boost::format(Y("Invalid track ID/file name specification in argument '%1%'.\n")) % args[i]);
        else
          mxerror(boost::format(Y("Invalid attachment ID/file name specification in argument '%1%'.\n")) % args[i]);

      track_spec_t track;
      memset(&track, 0, sizeof(track_spec_t));

      parse_int(matches[1].str(), track.tid);

      string output_file_name;
      if (matches[3].matched)
        output_file_name = matches[3].str();

      if (output_file_name.empty())
        if (MODE_ATTACHMENTS == mode)
          mxinfo(Y("No output file name specified, will use attachment name.\n"));
        else
          mxerror(boost::format(Y("Missing output file name in argument '%1%'.\n")) % args[i]);

      track.out_name               = safestrdup(output_file_name.c_str());
      track.sub_charset            = safestrdup(sub_charset);
      track.embed_in_ogg           = embed_in_ogg;
      track.extract_cuesheet       = extract_cuesheet;
      track.extract_blockadd_level = extract_blockadd_level;
      track.extract_raw            = extract_raw;
      tracks.push_back(track);

      sub_charset                  = "UTF-8";
      embed_in_ogg                 = true;
      extract_cuesheet             = false;
      extract_raw                  = 0;

    } else
      mxerror(boost::format(Y("Unrecognized command line option '%1%'. Maybe you put a mode specific option before the input file name?\n")) % args[i]);
  }

  if ((MODE_TAGS == mode) || (MODE_CHAPTERS == mode) || (MODE_CUESHEET == mode))
    return;

  if (tracks.empty()) {
    mxinfo(Y("Nothing to do.\n\n"));
    usage();
  }
}

void
show_element(EbmlElement *l,
             int level,
             const std::string &info) {
  if (9 < level)
    mxerror(boost::format(Y("mkvextract.cpp/show_element(): level > 9: %1%")) % level);

  if (0 == verbose)
    return;

  char level_buffer[10];
  memset(&level_buffer[1], ' ', 9);
  level_buffer[0]     = '|';
  level_buffer[level] = 0;

  mxinfo(boost::format("(%1%) %2%+ %3%") % NAME % level_buffer % info);
  if (NULL != l)
    mxinfo(boost::format(Y(" at %1%")) % l->GetElementPosition());
  mxinfo("\n");
}

void
show_error(const std::string &error) {
  mxinfo(boost::format("(%1%) %2%\n") % NAME % error);
}

int
main(int argc,
     char **argv) {
  init_stdio();
  set_usage();

#if defined(SYS_UNIX)
  nice(2);
#endif

#if defined(HAVE_LIBINTL_H)
  if (setlocale(LC_MESSAGES, "") == NULL)
    mxerror("The locale could not be set properly. Check the LANG, LC_ALL and LC_MESSAGES environment variables.\n");
  bindtextdomain("mkvtoolnix", MTX_LOCALE_DIR);
  textdomain("mkvtoolnix");
#endif

  mm_file_io_c::setup();
  srand(time(NULL));
  utf8_init("");

  xml_element_map_init();

  string input_file;
  operation_mode_e mode;
  vector<track_spec_t> tracks;

  parse_args(command_line_utf8(argc, argv), input_file, mode, tracks);
  if (MODE_TRACKS == mode) {
    extract_tracks(input_file.c_str(), tracks);

    if (0 == verbose)
      mxinfo(Y("Progress: 100%%\n"));

  } else if (MODE_TAGS == mode)
    extract_tags(input_file.c_str(), s_parse_fully);

  else if (MODE_ATTACHMENTS == mode)
    extract_attachments(input_file.c_str(), tracks, s_parse_fully);

  else if (MODE_CHAPTERS == mode)
    extract_chapters(input_file.c_str(), s_chapter_format_simple, s_parse_fully);

  else if (MODE_CUESHEET == mode)
    extract_cuesheet(input_file.c_str(), s_parse_fully);

  else if (MODE_TIMECODES_V2 == mode)
    extract_timecodes(input_file, tracks, 2);

  else
    mxerror(Y("mkvextract: Unknown mode!?\n"));

  utf8_done();

  return 0;

}
