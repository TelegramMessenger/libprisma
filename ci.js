#!/usr/bin/env node
const fs = require('fs');
const exec = require('child_process').execSync;

exec('npm i -g puppeteer --silent');
const npmRoot = exec('npm root -g --silent').toString().trim();
const puppeteer = require(`${npmRoot}/puppeteer`);

const waitFile = async filename => (await new Promise(async (resolve, _) => {
    if (!fs.existsSync(filename)) {
        await new Promise(resolve => setTimeout(resolve, 500));
        await waitFile(filename);
        resolve();
    } else {
        resolve();
    }
}));

let launchArgs = {headless: 'new'};
if (process.env.CI) {
    launchArgs = {
        args: ['--no-sandbox'],
        executablePath: process.env.PUPPETEER_EXEC_PATH, // set by docker container
        headless: false,
    }
}


(async () => {
    // Launch the browser and open a new blank page
    console.log('Launching browser')
    const browser = await puppeteer.launch(launchArgs);

    const page = await browser.newPage();
    await page.goto('https://prismjs.com/test.html');
    await page.setViewport({width: 1337, height: 1337});
    console.log('Page loaded')

    const client = await page.target().createCDPSession();
    await client.send('Browser.setDownloadBehavior', {
        behavior: 'allow',
        downloadPath: './libprisma'
    });

    // order is important
    const scriptsToEval = ['loadLangs.js', 'isEqual.js', 'generate.js']
    for (const script of scriptsToEval) {
        const content = fs.readFileSync(script, 'utf8');
        console.log(`Evaluating ${script}`)
        await page.evaluate(content);
    }

    fs.unlinkSync('./libprisma/grammars.dat')

    // hacky way to wait for the file to be downloaded
    // TODO: mb add timeout from script side instead of gha workflow
    console.log('Waiting for file')
    await waitFile('./libprisma/grammars.dat')

    console.log('Closing browser')
    await browser.close();
})();
