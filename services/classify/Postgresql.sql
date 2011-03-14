CREATE TABLE cicap_classify_token_data (
  classid INT,
  token BIGINT,
  hits INT,
  last_hit DATE,
  UNIQUE (classid, token)
) WITHOUT OIDS;

CREATE TABLE cicap_classify_classes (
  classid INT,
  name VARCHAR,
  UNIQUE(classid)
) WITHOUT OIDS;


CREATE OR REPLACE VIEW cicap_classify_token_data_bayes (token, classid, classname, bayes) AS

SELECT cicap_classify_token_data.token, cicap_classify_token_data.classid, cicap_classify_classes.name,  (.5 + CAST (cicap_classify_token_data.hits AS DOUBLE PRECISION) / (SELECT CAST(SUM(hits) AS DOUBLE PRECISION) FROM cicap_classify_token_data WHERE cicap_classify_token_data.token = sumaccess.token)) AS BAYES FROM cicap_classify_token_data
LEFT OUTER JOIN cicap_classify_classes ON (cicap_classify_token_data.classid = cicap_classify_classes.classid)
LEFT OUTER JOIN cicap_classify_token_data sumaccess ON (cicap_classify_token_data.classid = sumaccess.classid AND cicap_classify_token_data.token = sumaccess.token) ORDER BY cicap_classify_token_data.token, cicap_classify_token_data.classid;
