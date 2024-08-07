--TEST--
Test strftime() function : usage variation - Passing week related format strings to format argument.
--FILE--
<?php
echo "*** Testing strftime() : usage variation ***\n";

date_default_timezone_set("Asia/Calcutta");
// Initialise function arguments not being substituted (if any)
$timestamp = mktime(8, 8, 8, 8, 8, 2008);

//array of values to iterate over
$inputs = array(
      'Abbreviated weekday name' => "%a",
      'Full weekday name' => "%A",
      'Week number of the year' => "%U",
      'Week number of the year in decimal number' => "%W",
);
// loop through each element of the array for timestamp

foreach($inputs as $key =>$value) {
      echo "\n--$key--\n";
      var_dump( strftime($value) );
      var_dump( strftime($value, $timestamp) );
};

?>
--EXPECTF--
*** Testing strftime() : usage variation ***

--Abbreviated weekday name--

Deprecated: Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(%d) "%s"

Deprecated: Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(3) "Fri"

--Full weekday name--

Deprecated: Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(%d) "%s"

Deprecated: Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(6) "Friday"

--Week number of the year--

Deprecated: Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(%d) "%d"

Deprecated: Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(2) "31"

--Week number of the year in decimal number--

Deprecated: Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(%d) "%d"

Deprecated: Function strftime() is deprecated since 8.1, use IntlDateFormatter::format() instead in %s on line %d
string(2) "31"
