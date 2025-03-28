TEST_DIR=test_js
TESTS = $(wildcard $(TEST_DIR)/*.js) 

matchtest: $(TESTS)
	cd ../; ./driver $(TEST_DIR)/core_test.js | jq .
	#cd ../; ./driver $(TEST_DIR)/core_test.js | jq -r '.[]|select(.happy == false)|"\(.n): \(.case.title); wanted: \(.case.w) got: \(.got)"'
	#../driver match_test.js | tee match_test.results.json | jq -r '.[]|select(.happy == false)|"\(.n): \(.case.title); wanted: \(.case.w) got: \(.got)"'
	#cat match_test.results.json | jq -r '.[]|"\(.n): elapsed \(.bench.elapsed)ms (\(.bench.rounds) rounds) \(.case.title)"'

