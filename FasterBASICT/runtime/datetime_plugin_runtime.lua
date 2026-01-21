--
-- datetime_plugin_runtime.lua
-- DATETIME Plugin Runtime for FasterBASIC
--
-- Provides date/time manipulation functions using Lua's os.date/os.time
--

local M = {}

-- Helper: Day names
local DAY_NAMES = {
    [0] = "Sunday",
    [1] = "Monday",
    [2] = "Tuesday",
    [3] = "Wednesday",
    [4] = "Thursday",
    [5] = "Friday",
    [6] = "Saturday"
}

-- Helper: Month names
local MONTH_NAMES = {
    [1] = "January",
    [2] = "February",
    [3] = "March",
    [4] = "April",
    [5] = "May",
    [6] = "June",
    [7] = "July",
    [8] = "August",
    [9] = "September",
    [10] = "October",
    [11] = "November",
    [12] = "December"
}

-- Helper: Days in each month (non-leap year)
local DAYS_IN_MONTH = {
    [1] = 31,
    [2] = 28,
    [3] = 31,
    [4] = 30,
    [5] = 31,
    [6] = 30,
    [7] = 31,
    [8] = 31,
    [9] = 30,
    [10] = 31,
    [11] = 30,
    [12] = 31
}

-- Helper: Check if year is a leap year
local function is_leap_year(year)
    if year % 400 == 0 then
        return true
    elseif year % 100 == 0 then
        return false
    elseif year % 4 == 0 then
        return true
    else
        return false
    end
end

-- Helper: Get days in month
local function days_in_month(year, month)
    if month == 2 and is_leap_year(year) then
        return 29
    end
    return DAYS_IN_MONTH[month] or 0
end

-- Helper: Validate date components
local function is_valid_date(year, month, day)
    if year < 1970 or year > 2038 then return false end
    if month < 1 or month > 12 then return false end
    if day < 1 or day > days_in_month(year, month) then return false end
    return true
end

-- Helper: Convert format string from strftime-like to Lua os.date format
-- Supports common format codes
local function convert_format(fmt)
    -- Lua's os.date supports these directly:
    -- %Y year, %m month, %d day, %H hour, %M minute, %S second
    -- %w weekday (0=Sunday), %j day of year, %b month name, %B full month
    -- %a weekday name, %A full weekday, %I 12-hour, %p AM/PM
    return fmt
end

-- DTNOW() - Get current Unix timestamp
function dt_now()
    return os.time()
end

-- DTFORMAT(timestamp, format$) - Format timestamp to string
function dt_format(timestamp, fmt)
    if not timestamp or not fmt then
        error("DTFORMAT requires timestamp and format")
    end

    -- Convert to number if string
    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTFORMAT: invalid timestamp")
    end

    -- Convert format and apply
    local lua_fmt = convert_format(fmt)
    local success, result = pcall(os.date, lua_fmt, timestamp)

    if success then
        return result
    else
        error("DTFORMAT: invalid format string")
    end
end

-- DTPARSE(datestring$, format$) - Parse date string to timestamp
-- Note: Lua doesn't have built-in strptime, so we handle common formats
function dt_parse(datestr, fmt)
    if not datestr or not fmt then
        error("DTPARSE requires date string and format")
    end

    -- Handle common ISO format: YYYY-MM-DD
    if fmt == "%Y-%m-%d" then
        local year, month, day = datestr:match("^(%d%d%d%d)%-(%d%d)%-(%d%d)$")
        if year and month and day then
            return os.time({ year = tonumber(year), month = tonumber(month), day = tonumber(day), hour = 0, min = 0, sec = 0 })
        end
    end

    -- Handle ISO datetime: YYYY-MM-DD HH:MM:SS
    if fmt == "%Y-%m-%d %H:%M:%S" then
        local year, month, day, hour, min, sec = datestr:match("^(%d%d%d%d)%-(%d%d)%-(%d%d) (%d%d):(%d%d):(%d%d)$")
        if year then
            return os.time({
                year = tonumber(year),
                month = tonumber(month),
                day = tonumber(day),
                hour = tonumber(hour),
                min = tonumber(min),
                sec = tonumber(sec)
            })
        end
    end

    -- Handle US format: MM/DD/YYYY
    if fmt == "%m/%d/%Y" then
        local month, day, year = datestr:match("^(%d%d)/(%d%d)/(%d%d%d%d)$")
        if year and month and day then
            return os.time({ year = tonumber(year), month = tonumber(month), day = tonumber(day), hour = 0, min = 0, sec = 0 })
        end
    end

    -- Handle European format: DD/MM/YYYY
    if fmt == "%d/%m/%Y" then
        local day, month, year = datestr:match("^(%d%d)/(%d%d)/(%d%d%d%d)$")
        if year and month and day then
            return os.time({ year = tonumber(year), month = tonumber(month), day = tonumber(day), hour = 0, min = 0, sec = 0 })
        end
    end

    -- Handle DD.MM.YYYY
    if fmt == "%d.%m.%Y" then
        local day, month, year = datestr:match("^(%d%d)%.(%d%d)%.(%d%d%d%d)$")
        if year and month and day then
            return os.time({ year = tonumber(year), month = tonumber(month), day = tonumber(day), hour = 0, min = 0, sec = 0 })
        end
    end

    error("DTPARSE: unsupported format or invalid date string")
end

-- DTADD(timestamp, amount, unit$) - Add time to timestamp
function dt_add(timestamp, amount, unit)
    if not timestamp or not amount or not unit then
        error("DTADD requires timestamp, amount, and unit")
    end

    timestamp = tonumber(timestamp)
    amount = tonumber(amount)

    if not timestamp or not amount then
        error("DTADD: invalid timestamp or amount")
    end

    unit = unit:lower()

    local multiplier
    if unit == "second" or unit == "seconds" then
        multiplier = 1
    elseif unit == "minute" or unit == "minutes" then
        multiplier = 60
    elseif unit == "hour" or unit == "hours" then
        multiplier = 3600
    elseif unit == "day" or unit == "days" then
        multiplier = 86400
    elseif unit == "week" or unit == "weeks" then
        multiplier = 604800
    elseif unit == "month" or unit == "months" then
        -- Month addition is more complex - need to handle variable days
        local date = os.date("*t", timestamp)
        date.month = date.month + amount

        -- Handle year overflow/underflow
        while date.month > 12 do
            date.month = date.month - 12
            date.year = date.year + 1
        end
        while date.month < 1 do
            date.month = date.month + 12
            date.year = date.year - 1
        end

        -- Handle day overflow (e.g., Jan 31 + 1 month = Feb 28/29)
        local max_day = days_in_month(date.year, date.month)
        if date.day > max_day then
            date.day = max_day
        end

        return os.time(date)
    elseif unit == "year" or unit == "years" then
        local date = os.date("*t", timestamp)
        date.year = date.year + amount

        -- Handle leap year edge case (Feb 29 -> Feb 28)
        if date.month == 2 and date.day == 29 and not is_leap_year(date.year) then
            date.day = 28
        end

        return os.time(date)
    else
        error("DTADD: invalid unit (use seconds/minutes/hours/days/weeks/months/years)")
    end

    return timestamp + (amount * multiplier)
end

-- DTDIFF(timestamp1, timestamp2, unit$) - Calculate difference
function dt_diff(ts1, ts2, unit)
    if not ts1 or not ts2 or not unit then
        error("DTDIFF requires two timestamps and a unit")
    end

    ts1 = tonumber(ts1)
    ts2 = tonumber(ts2)

    if not ts1 or not ts2 then
        error("DTDIFF: invalid timestamps")
    end

    local diff_seconds = ts1 - ts2
    unit = unit:lower()

    if unit == "second" or unit == "seconds" then
        return diff_seconds
    elseif unit == "minute" or unit == "minutes" then
        return math.floor(diff_seconds / 60)
    elseif unit == "hour" or unit == "hours" then
        return math.floor(diff_seconds / 3600)
    elseif unit == "day" or unit == "days" then
        return math.floor(diff_seconds / 86400)
    elseif unit == "week" or unit == "weeks" then
        return math.floor(diff_seconds / 604800)
    else
        error("DTDIFF: invalid unit (use seconds/minutes/hours/days/weeks)")
    end
end

-- DTGET(timestamp, component$) - Extract component
function dt_get(timestamp, component)
    if not timestamp or not component then
        error("DTGET requires timestamp and component")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTGET: invalid timestamp")
    end

    local date = os.date("*t", timestamp)
    component = component:lower()

    if component == "year" then
        return date.year
    elseif component == "month" then
        return date.month
    elseif component == "day" then
        return date.day
    elseif component == "hour" then
        return date.hour
    elseif component == "minute" or component == "min" then
        return date.min
    elseif component == "second" or component == "sec" then
        return date.sec
    elseif component == "wday" or component == "weekday" then
        return date.wday - 1 -- Convert to 0=Sunday
    elseif component == "yday" or component == "yearday" then
        return date.yday
    else
        error("DTGET: invalid component (use year/month/day/hour/minute/second/wday/yday)")
    end
end

-- DTMAKE(year, month, day, hour, minute, second) - Create timestamp
function dt_make(year, month, day, hour, minute, second)
    if not year or not month or not day then
        error("DTMAKE requires at least year, month, and day")
    end

    year = tonumber(year)
    month = tonumber(month)
    day = tonumber(day)
    hour = tonumber(hour) or 0
    minute = tonumber(minute) or 0
    second = tonumber(second) or 0

    if not is_valid_date(year, month, day) then
        error("DTMAKE: invalid date")
    end

    if hour < 0 or hour > 23 then
        error("DTMAKE: invalid hour (0-23)")
    end

    if minute < 0 or minute > 59 then
        error("DTMAKE: invalid minute (0-59)")
    end

    if second < 0 or second > 59 then
        error("DTMAKE: invalid second (0-59)")
    end

    return os.time({ year = year, month = month, day = day, hour = hour, min = minute, sec = second })
end

-- DTDAYNAME(timestamp) - Get day name
function dt_dayname(timestamp)
    if not timestamp then
        error("DTDAYNAME requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTDAYNAME: invalid timestamp")
    end

    local wday = tonumber(os.date("%w", timestamp))
    return DAY_NAMES[wday]
end

-- DTMONTHNAME(timestamp) - Get month name
function dt_monthname(timestamp)
    if not timestamp then
        error("DTMONTHNAME requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTMONTHNAME: invalid timestamp")
    end

    local month = tonumber(os.date("%m", timestamp))
    return MONTH_NAMES[month]
end

-- DTWEEKNUM(timestamp) - Get ISO week number
function dt_weeknum(timestamp)
    if not timestamp then
        error("DTWEEKNUM requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTWEEKNUM: invalid timestamp")
    end

    -- ISO week calculation
    -- Week 1 is the week with the first Thursday of the year
    local date = os.date("*t", timestamp)
    local jan1 = os.time({ year = date.year, month = 1, day = 1, hour = 0, min = 0, sec = 0 })
    local jan1_wday = tonumber(os.date("%w", jan1))

    -- Days since Jan 1
    local day_of_year = date.yday

    -- Adjust for ISO week (Monday = 1)
    local iso_wday = (jan1_wday + 6) % 7 -- 0=Monday, 6=Sunday

    -- Calculate week number
    local week = math.floor((day_of_year + iso_wday - 1) / 7) + 1

    -- Handle edge cases for first/last week
    if week == 0 then
        -- Belongs to last week of previous year
        return 52 -- Simplified; could be 53
    elseif week == 53 then
        -- Check if actually week 1 of next year
        local dec31_wday = (jan1_wday + (is_leap_year(date.year) and 365 or 364)) % 7
        if dec31_wday < 4 then
            return 1
        end
    end

    return week
end

-- DTISLEAP(year) - Check if leap year
function dt_isleap(year)
    if not year then
        error("DTISLEAP requires year")
    end

    year = tonumber(year)
    if not year then
        error("DTISLEAP: invalid year")
    end

    return is_leap_year(year) and 1 or 0
end

-- DTVALID(year, month, day) - Validate date
function dt_valid(year, month, day)
    if not year or not month or not day then
        return 0
    end

    year = tonumber(year)
    month = tonumber(month)
    day = tonumber(day)

    if not year or not month or not day then
        return 0
    end

    return is_valid_date(year, month, day) and 1 or 0
end

-- DTDAYSINMONTH(year, month) - Get days in month
function dt_daysinmonth(year, month)
    if not year or not month then
        error("DTDAYSINMONTH requires year and month")
    end

    year = tonumber(year)
    month = tonumber(month)

    if not year or not month or month < 1 or month > 12 then
        error("DTDAYSINMONTH: invalid year or month")
    end

    return days_in_month(year, month)
end

-- DTCOMPARE(timestamp1, timestamp2) - Compare timestamps
function dt_compare(ts1, ts2)
    if not ts1 or not ts2 then
        error("DTCOMPARE requires two timestamps")
    end

    ts1 = tonumber(ts1)
    ts2 = tonumber(ts2)

    if not ts1 or not ts2 then
        error("DTCOMPARE: invalid timestamps")
    end

    if ts1 < ts2 then
        return -1
    elseif ts1 > ts2 then
        return 1
    else
        return 0
    end
end

-- DTUTCOFFSET() - Get UTC offset in seconds
function dt_utcoffset()
    local now = os.time()
    local utc = os.time(os.date("!*t", now))
    return os.difftime(now, utc)
end

-- DTTOUTC(local_timestamp) - Convert local to UTC
function dt_toutc(timestamp)
    if not timestamp then
        error("DTTOUTC requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTTOUTC: invalid timestamp")
    end

    local offset = dt_utcoffset()
    return timestamp - offset
end

-- DTFROMUTC(utc_timestamp) - Convert UTC to local
function dt_fromutc(timestamp)
    if not timestamp then
        error("DTFROMUTC requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTFROMUTC: invalid timestamp")
    end

    local offset = dt_utcoffset()
    return timestamp + offset
end

-- DTSTARTOFDAY(timestamp) - Get start of day
function dt_startofday(timestamp)
    if not timestamp then
        error("DTSTARTOFDAY requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTSTARTOFDAY: invalid timestamp")
    end

    local date = os.date("*t", timestamp)
    date.hour = 0
    date.min = 0
    date.sec = 0

    return os.time(date)
end

-- DTENDOFDAY(timestamp) - Get end of day
function dt_endofday(timestamp)
    if not timestamp then
        error("DTENDOFDAY requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTENDOFDAY: invalid timestamp")
    end

    local date = os.date("*t", timestamp)
    date.hour = 23
    date.min = 59
    date.sec = 59

    return os.time(date)
end

-- DTSTARTOFWEEK(timestamp) - Get start of week (Monday)
function dt_startofweek(timestamp)
    if not timestamp then
        error("DTSTARTOFWEEK requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTSTARTOFWEEK: invalid timestamp")
    end

    local date = os.date("*t", timestamp)
    local wday = date.wday

    -- Convert to days since Monday (0=Monday)
    local days_since_monday = (wday + 5) % 7

    -- Go back to Monday
    local monday = timestamp - (days_since_monday * 86400)

    -- Set to start of day
    return dt_startofday(monday)
end

-- DTSTARTOFMONTH(timestamp) - Get start of month
function dt_startofmonth(timestamp)
    if not timestamp then
        error("DTSTARTOFMONTH requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTSTARTOFMONTH: invalid timestamp")
    end

    local date = os.date("*t", timestamp)
    date.day = 1
    date.hour = 0
    date.min = 0
    date.sec = 0

    return os.time(date)
end

-- DTSTARTOFYEAR(timestamp) - Get start of year
function dt_startofyear(timestamp)
    if not timestamp then
        error("DTSTARTOFYEAR requires timestamp")
    end

    timestamp = tonumber(timestamp)
    if not timestamp then
        error("DTSTARTOFYEAR: invalid timestamp")
    end

    local date = os.date("*t", timestamp)
    date.month = 1
    date.day = 1
    date.hour = 0
    date.min = 0
    date.sec = 0

    return os.time(date)
end

-- Export all functions
return {
    dt_now = dt_now,
    dt_format = dt_format,
    dt_parse = dt_parse,
    dt_add = dt_add,
    dt_diff = dt_diff,
    dt_get = dt_get,
    dt_make = dt_make,
    dt_dayname = dt_dayname,
    dt_monthname = dt_monthname,
    dt_weeknum = dt_weeknum,
    dt_isleap = dt_isleap,
    dt_valid = dt_valid,
    dt_daysinmonth = dt_daysinmonth,
    dt_compare = dt_compare,
    dt_utcoffset = dt_utcoffset,
    dt_toutc = dt_toutc,
    dt_fromutc = dt_fromutc,
    dt_startofday = dt_startofday,
    dt_endofday = dt_endofday,
    dt_startofweek = dt_startofweek,
    dt_startofmonth = dt_startofmonth,
    dt_startofyear = dt_startofyear
}
