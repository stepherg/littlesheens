.SILENT:

TEST_DIR=test_js
TESTS = $(wildcard $(TEST_DIR)/*.js) 

matchtest: $(TESTS)
	#cd ../; ./driver $(TEST_DIR)/common.js $(TEST_DIR)/core_test.js 
	cd ../; ./driver $(TEST_DIR)/common.js $(TEST_DIR)/core_test.js | tee core_test.results.json | jq -r '.[]|select(.happy == false)|"\(.n): \(.case.title); wanted: \(.case.w) got: \(.bss)"'
	cd ../; cat core_test.results.json | jq -r '.[]|"\(.n): elapsed \(.bench.elapsed)ms (\(.bench.rounds) rounds) \(.case.title)"'
#	cd ../; ./driver $(TEST_DIR)/common.js $(TEST_DIR)/match_test.js | tee match_test.results.json | jq -r '.[]|select(.happy == false)|"\(.n): \(.case.title); wanted: \(.case.w) got: \(.got)"'
#	cd ../; cat match_test.results.json | jq -r '.[]|"\(.n): elapsed \(.bench.elapsed)ms (\(.bench.rounds) rounds) \(.case.title)"'

