#include <Arduino.h>
#include "hdate.h"

#define DISP_WIDTH 40
#define DISP_HEIGHT 4

// These are the lengths we allocate for various types of strings
// plus their surrounding markes
#define DATE_BUF_LEN 20
#define PARSHA_BUF_LEN (DISP_WIDTH - DATE_BUF_LEN)
#define CHAG_BUF_LEN 20
#define CHAD_DATE_BUF_LEN (DISP_WIDTH - CHAG_BUF_LEN)

// TODO: The following things are hardcoded and should be got from
// either config or external sources!
#define DIASPORA true
#define LATITUDE -33.865143
#define LONGITUDE 151.209900
#define DAY 7
#define MONTH 4
#define YEAR 2024
// Equiv of 10:30 in hours since midnight
#define TIME ((10 * 60) + 30)

// put function declarations here:
int myFunction(int, int);
hdate_struct h;
hdate_struct next_holyday;
hdate_struct next_shabbos;

int parsha;
int holyday;
char dateBuf[DATE_BUF_LEN];

// Globals buffer to hold the display
// Extra bytes added to each length for the null termination.
char dispBuf[DISP_HEIGHT][DISP_WIDTH + 1];

// We hold a hallachic time as both the time and it's id.
struct h_time
{
  char id[3]; // 3 Charachter ID for the time
  int time;   // The time of the time in minutes from midnight.
};

// To index into array we use each of the IDs of each time value
// TODO: Will have to undo this if I get around to dynamically
// adjusting list of times to the type of day
// Also could use X() macros for this.
enum timeId
{
  AHa = 0,
  Msh,
  HHa,
  LSh,
  LTf,
  CHt,
  MGd,
  MKt,
  PlM,
  CnL,
  Shk,
  BHs,
  TzH,
  NumTimeIds
};

#define TIMES_LEN NumTimeIds

// When printing strings we use the form "XX:XX|" which is 6 chars long
// This each time buffer is 6 chars long. The header above it is also
// 6 chars long and so looks like "XXX  |"
#define TB_LEN 6

// Time is held as integer of minutes from midnight.
h_time times[TIMES_LEN];

int sun_hour;
int first_light;
int talit;
int sunrise;
int midday;
int sunset;
int first_stars;
int three_stars;

int hdate_get_next_holyday(hdate_struct *h, hdate_struct *next_h);
int hdate_get_next_shabbos(hdate_struct *h, hdate_struct *next_h);

void setup()
{
  Serial.begin(115200);

  // Yes I could use X() macros instead. But it's a short list and
  // I'm pretty sure I'll make it more dynamic later.
  memcpy(times[AHa].id, "AHa", 3);
  memcpy(times[Msh].id, "Msh", 3);
  memcpy(times[HHa].id, "HHa", 3);
  memcpy(times[LSh].id, "LSh", 3);
  memcpy(times[LTf].id, "LTf", 3);
  memcpy(times[CHt].id, "CHt", 3);
  memcpy(times[MGd].id, "MGd", 3);
  memcpy(times[MKt].id, "MKt", 3);
  memcpy(times[PlM].id, "PlM", 3);
  memcpy(times[CnL].id, "CnL", 3);
  memcpy(times[Shk].id, "Shk", 3);
  memcpy(times[BHs].id, "BHs", 3);
  memcpy(times[TzH].id, "TzH", 3);
}

void loop()
{
  // Get the hebrew date for today into the hdate_struct data struct
  hdate_set_gdate(&h, DAY, MONTH, YEAR);
  int curTime = TIME;
  int chatzot = 0;

  char *date;
  /*
   * This program is written for a 40x4 matrix display.
   *
   * The first two lines stay like this constantly
   *
   *   0        1        2        3
   *   012345689012345689012345689012345689
   * 0 [Hebrew Date      ][        Parasha]
   * 1 [Next Chag        ][ Next Chag Date]
   *
   */
  date = hdate_get_format_date(&h, dateBuf, DATE_BUF_LEN, DIASPORA, false);
  parsha = hdate_get_next_shabbos(&h, &next_shabbos);
  holyday = hdate_get_next_holyday(&h, &next_holyday);

  // We always use memcpy here as we don't want to whack in trailing zeros
  // when we're laying out the text.
  snprintf(dispBuf[0], DISP_WIDTH + 1, "%s|%s", date, hdate_get_parasha_string(parsha, true));

  date = hdate_get_format_date(&next_holyday, dateBuf, DATE_BUF_LEN, DIASPORA, false);

  snprintf(
      dispBuf[1],
      DISP_WIDTH + 1,
      "%s|%s",
      hdate_get_holyday_string(holyday, true),
      date);

  /*
   * The bottom two lines show the next 6 upcoming hallachic times as shown
   * below. The top row identified the upcoming time and the bottom the
   * actual times.
   *
   * TODO: The actual times inserted will depend on whether they are hallachnically
   * meaningful for that day. (Or they will be when I code that bit.)
   *
   *   0        1        2        3
   *   012345689012345689012345689012345689
   * 2 AHa  |MGd  |
   * 3 XX:XX|XX:XX|XX:XX|XX:XX|XX:XX|XX:XX
   *
   */

  hdate_get_utc_sun_time_full(
      hdate_get_gday(&h),
      hdate_get_gmonth(&h),
      hdate_get_gyear(&h),
      LATITUDE,
      LONGITUDE,
      &sun_hour,
      &first_light,
      &talit,
      &sunrise,
      &midday,
      &sunset,
      &first_stars,
      &three_stars);

  /* The Hallachic Times are as below - Second columm is the
   *   variable we get from the hdate library they correspond to
   *
   * SHr (sun_hour)                    = Shaah Zmanit (Hallachic Hour)
   * AHa (first_light)                  = Alot Haschar
   * Msh (talit)                        = Misheyakir - Earliest time for Tallit and Tefillin:
   * HHa (sunrise)                      = Hanetz HaChamah/Sunrise
   * LSh (first_light + 3xSHr)          = Latest Shema
   * LTf (first_light + 4xSHr)          = Latest Tefillah
   * CHt (sunrise + (sunset-Sunrise)/2) = Chatzot/Midday
   * MGd (Cht + SHr/2)                  = Mincha Gedolah
   * MKt (SHk - 2.5*SHr)                = Minha Ketan
   * PlM (SHk - 1.25*Shr)               = Plag HaMincha
   * CnL (SHk - 18)                     = Candle Lighting
   * SHk (sunset)                       = Shekiah
   * BHs (first_stars)                  = Bein Hashmashot - Twilight
   * TzH (three_stars)                  = Tzeit Hakochavim
   */
  times[AHa].time = first_light;
  times[Msh].time = talit;
  times[HHa].time = sunrise;
  times[LSh].time = first_light + (3 * sun_hour);
  times[LTf].time = first_light + (4 * sun_hour);
  times[CHt].time = (chatzot = (sunrise + (sunset - sunrise) / 2));
  times[MGd].time = chatzot + (sun_hour / 2);
  times[MKt].time = sunset - sun_hour / 2;
  times[PlM].time = sunset - (sun_hour * 1.25);
  times[CnL].time = sunset - 18;
  times[Shk].time = sunset;
  times[BHs].time = first_stars;
  times[TzH].time = three_stars;

  /*
   * Now to find which items to display we'll iterate over the list of times
   * until we are past the current time, and then display the next 6 times.
   * If we ever get to Mincha Katan however we just display from there as
   * there are only 6 left.
   */

  int latestTimeId = 0;
  while (
      times[latestTimeId].time < curTime &&
      (TIMES_LEN - latestTimeId) < 6)
  {
    latestTimeId++;
  }

  char timeBuf[TB_LEN];
  for (int i = 0; i++; i <= 6)
  {
    // Copy the Id to the 3rd row
    snprintf(
        dispBuf[2] + (i * TB_LEN),
        TB_LEN,
        "%3.3s  |",
        times[latestTimeId + i].id);

    // Copy the time to the 4th row
    snprintf(
        dispBuf[3] + (i * TB_LEN),
        TB_LEN,
        "%2.2d:%2.2d|",
        times[latestTimeId].time / 60,
        times[latestTimeId].time % 60);

    Serial.println(dispBuf[0]);
    Serial.println(dispBuf[1]);
    Serial.println(dispBuf[2]);
    Serial.println(dispBuf[3]);
  }

  delay(2000);
  // put your main code here, to run repeatedly:
}

// Get next holyday, truening the holyday ID
int hdate_get_next_holyday(hdate_struct *h, hdate_struct *next_h)
{
  int holyday;
  *next_h = *h; // Copy all the items over;

  // Iterate over checking for a holyday (i.e. not return 0)
  while ((holyday = hdate_get_holyday(next_h, true)) == 0)
  {
    // Get the next day by checking the JD
    int jd = hdate_get_julian(next_h);
    jd = jd + 1;
    hdate_set_jd(next_h, jd);
  }

  return holyday;
}

// Get next sabbos returning the shabbos is
int hdate_get_next_shabbos(hdate_struct *h, hdate_struct *next_h)
{
  int parsha;

  *next_h = *h; // Copy all the items over;

  // Iterate over checking for a holyday (i.e. not return 0)
  while ((parsha = hdate_get_parasha(next_h, true)) == 0)
  {
    // Get the next day by checking the JD
    int jd = hdate_get_julian(next_h);
    jd = jd + 1;
    hdate_set_jd(next_h, jd);
  }

  return parsha;
}
