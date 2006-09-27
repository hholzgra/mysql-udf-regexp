-- disable_warnings
--error 0, 1128, 1305
DROP FUNCTION regexp_like;
CREATE FUNCTION regexp_like RETURNS INTEGER SONAME "regexp.so";
--error 0, 1128, 1305
DROP FUNCTION regexp_substr;
CREATE FUNCTION regexp_substr RETURNS STRING SONAME "regexp.so";
--error 0, 1128, 1305
DROP FUNCTION regexp_instr;
CREATE FUNCTION regexp_instr RETURNS INTEGER SONAME "regexp.so";
--error 0, 1128, 1305
DROP FUNCTION regexp_replace;
CREATE FUNCTION regexp_replace RETURNS STRING SONAME "regexp.so";
-- enable_warnings
