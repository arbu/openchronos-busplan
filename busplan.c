#include <stdlib.h>
#include <stdint.h>

#include "messagebus.h"
#include "menu.h"

#include "drivers/rtca.h"
#include "drivers/display.h"

#include "busplan_data.h"

#define SUN 0
#define MON 1
#define TUE 2
#define WED 3
#define THU 4
#define FRI 5
#define SAT 6

/* Holidays in Saxonia (excluding easter and depending days) */
#define feiertag(day, month, dow) (\
         (day == 1 && month == 1) ||                                /* Neujahr                   */\
         (day == 1 && month == 5) ||                                /* Tag der Arbeit            */\
         (day == 3 && month == 10) ||                               /* Tag der deutschen Einheit */\
         (dow == WED && (15 < day) && (day < 23) && month == 11) || /* Buss und Bettag           */\
         (day == 25 && month == 12) ||                              /* 1. Weihnachtsfeiertag     */\
         (day == 26 && month == 12)                                 /* 2. Weihnachtsfeiertag     */\
        )

enum sections section = 0;

enum sections get_section_by_date(uint8_t day, uint8_t month, uint8_t dow) {
    if(dow == SUN || feiertag(day, month, dow))
        return SONNundFEIERTAG;
    else if(dow == SAT)
        return SONNABEND;
    else
        return MONTAGbisFREITAG;
}

void set_section(enum sections new_section) {
    section = new_section;
}

void set_section_by_date(uint8_t day, uint8_t month, uint8_t dow, uint8_t hour) {
    if(hour < 4) { // before 4 o'clock the timetable of the previous day applies
        if(dow == 0)
            dow += 7;
        dow--;
        if(day == 0) {
            if(month == 0)
                month += 12;
            month--;
            day = 30; // just hope it's not a holiday
        }
    }
    section = get_section_by_date(day, month, dow);
    for(uint8_t i = 0; i < table_count; i++) {
        display_symbol(i, LCD_UNIT_L1_PER_S, section == SONNABEND ? SEG_ON : SEG_OFF);
        display_symbol(i, LCD_UNIT_L1_PER_H, section == SONNundFEIERTAG ? SEG_ON : SEG_OFF);
    }
}

struct meta get_meta(uint8_t table) {
    return meta_plan[table];
}

uint8_t next_bus(uint8_t table, uint8_t hour, uint8_t minute) {
    const uint8_t *diffs = plan[table][section];
    uint16_t last = 0;
    if(hour < 4)
        hour += 24;
    uint16_t time = (hour - 4) * 60 + minute;
    for(uint8_t i = 0; diffs[i] != 0; i++) {
        uint8_t count = 1;
        if(diffs[i] & 128) {
            count = diffs[i] & 127;
            if(count == 0) {
                last += 128;
                continue;
            }
            i++;
        }
        while(count--) {
            last += diffs[i];
            if(last >= time) {
                if(last - time > 255)
                    return 255;
                return last - time;
            }
        }
    }
    // no bus found so it must be the first in the list
    if(hour > 4)
        return 255;
    diffs = plan[table][get_section_by_date(rtca_time.day, rtca_time.mon, rtca_time.dow)];
    uint8_t i = 0;
    uint16_t first = 0;
    while(diffs[i] == 128) {
        first += 128;
        i++;
    }
    if(diffs[i] & 128)
        i++;
    first += diffs[i];
    if(first - time > 255)
        return 255;
    return first - time;
}

void update_times(enum sys_message message) {
    if(message & SYS_MSG_RTC_HOUR && rtca_time.hour == 4)
        set_section_by_date(rtca_time.day, rtca_time.mon, rtca_time.dow, rtca_time.hour);
    if(message & SYS_MSG_RTC_MINUTE) {
        for(uint8_t i = 0; i < table_count; i++) {
            uint8_t diff = next_bus(i, rtca_time.hour, rtca_time.min);
            if(diff < 100)
                _printf(i, LCD_SEG_L2_1_0, "%2u", diff);
            else
                display_chars(i, LCD_SEG_L2_1_0, "--", SEG_SET);
        }
    }
}

void activate_busplan() {
    lcd_screens_create(table_count);
    for(uint8_t i = 0; i < table_count; i++) {
        struct meta meta = get_meta(i);
        display_chars(i, LCD_SEG_L1_1_0, meta.to, SEG_SET);
        display_chars(i, LCD_SEG_L2_4_2, meta.from, SEG_SET);
        _printf(i, LCD_SEG_L1_3_2, "%2u", meta.route);
        display_symbol(i, LCD_SEG_L1_COL, SEG_ON);
        display_symbol(i, LCD_SEG_L2_COL0, SEG_ON);
    }
    update_times(SYS_MSG_RTC_HOUR | SYS_MSG_RTC_MINUTE);
    sys_messagebus_register(&update_times, SYS_MSG_RTC_MINUTE | SYS_MSG_RTC_HOUR);
}

void deactivate_busplan() {
    sys_messagebus_unregister(&update_times, SYS_MSG_RTC_MINUTE | SYS_MSG_RTC_HOUR);
    lcd_screens_destroy();
    display_clear(0, 0);
}

void prev_table() {
    lcd_screen_activate((get_active_lcd_screen_nr() + table_count - 1) % table_count);
}

void next_table() {
    lcd_screen_activate(0xFF);
}

void mod_busplan_init() {
    menu_add_entry(
            "  BUS",
            &next_table,
            &prev_table,
            NULL,
            NULL,
            NULL,
            NULL,
            &activate_busplan,
            &deactivate_busplan
    );
};
