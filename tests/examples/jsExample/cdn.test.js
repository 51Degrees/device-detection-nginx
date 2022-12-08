/* *********************************************************************
 * This Original Work is copyright of 51 Degrees Mobile Experts Limited.
 * Copyright 2019 51 Degrees Mobile Experts Limited, 5 Charlotte Close,
 * Caversham, Reading, Berkshire, United Kingdom RG4 7BY.
 *
 * This Original Work is licensed under the European Union Public Licence (EUPL)
 * v.1.2 and is subject to its terms as set out below.
 *
 * If a copy of the EUPL was not distributed with this file, You can obtain
 * one at https://opensource.org/licenses/EUPL-1.2.
 *
 * The 'Compatible Licences' set out in the Appendix to the EUPL (as may be
 * amended by the European Commission) shall be deemed incompatible for
 * the purposes of the Work and the provisions of the compatibility
 * clause in Article 5 of the EUPL shall not apply.
 *
 * If using the Work as, or as part of, a network application, by
 * including the attribution notice(s) required under Article 5 of the EUPL
 * in the end user terms of the application under an appropriate heading,
 * such notice(s) shall fulfill the requirements of that article.
 * ********************************************************************* */

const {Builder, By} = require('selenium-webdriver');
const chrome = require('selenium-webdriver/chrome');
const firefox = require('selenium-webdriver/firefox');
const edge = require('selenium-webdriver/edge');
const path = require('path');
const fs = require('fs');

let cOpts = new chrome.Options();
let ffOpts = new firefox.Options();
let eOpts = new edge.Options();

async function cdnTest(driver) {
	try {
		let filePath =
			path.resolve('../../../examples/hash/jsExample/index.html');
		// Navigate to Url
		await driver.get('file://' + filePath);
		expect.stringMatching(/^\d+$/);
	}
	finally {
		await driver.quit();
	}
}

// Check CDN example work as expected. The nginx is required to be
// running with the example configuration file. Browsers and their
// drivers also need to be installed to run these tests.
describe('Javascript tests', () => { 
	test('Javascript in Chrome', async () => {
		let driver = await new Builder()
			.forBrowser('chrome')
			.setChromeOptions(cOpts.headless())
			.build();
		await cdnTest(driver);
	}, 20000);
	
	test('Javascript in Firefox', async () => {
		let driver = await new Builder()
			.forBrowser('firefox')
			.setFirefoxOptions(ffOpts.headless())
			.build();
		await cdnTest(driver);
	}, 20000);
	
	// Edge 108 is currently has issues starting in headless mode.
	// This test will be reinstated once the problem is solved.
	// test('Javascript in Microsoft Edge', async () => {
	// 	let driver = await new Builder()
	// 		.forBrowser('MicrosoftEdge')
	// 		.setEdgeOptions(eOpts.headless())
	// 		.build();
	// 	await cdnTest(driver);
	// }, 20000);
});
