--TEST--
Detailed reporting on specific types of syntax errors
--FILE--
<?php
$badCode = [
  "[1, 2,",                     /* unclosed [ */
  "if(1) { echo 'hello'; ",     /* unclosed { */
  ];

foreach ($badCode as $code) {
  try {
    eval($code);
  } catch (ParseError $e) {
    echo $e->getMessage(), "\n";
  }
}

echo "==DONE==\n";
?>
--EXPECT--
Unclosed '('
Unclosed '['
Unclosed '{'
Unmatched ')'
Unmatched ']'
Unmatched '}'
Unclosed '(' does not match ']'
Unclosed '[' does not match ')'
Unclosed '{' does not match ')'
Unclosed '{' on line 1
Unclosed '[' on line 1
Unclosed '{' on line 1
Unmatched ')'
Unmatched ']'
Unmatched '}'
Unclosed '(' on line 1 does not match ']'
Unclosed '[' on line 1 does not match ')'
Unclosed '{' on line 1 does not match ')'
==DONE==
