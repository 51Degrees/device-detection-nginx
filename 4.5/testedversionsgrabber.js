
var gitHubImageRows = undefined;
var gitHubLinks = undefined;

/**
 * Populate the gitHubImageRows and gitHubLinks synchronously, so that when
 * this function returns, the variables are populated.
 */
function populateImages() {
    $.ajax({
        url: 'https://raw.githubusercontent.com/actions/runner-images/main/README.md',
        success: function(markdown) {
            var imagesSection = getImagesSection(markdown);
            gitHubImageRows = getImageRows(imagesSection);
            var linksSection = getLinksSection(markdown);
            gitHubLinks = getLinks(linksSection);
        },
        async: false
    });
}

/**
 * Get the platform title with an HTML link to its definition
 * using the image name (including square brackets).
 * @param {string} image name of the image
 * @returns HTML link
 */
function getPlatform(image) {
    if (gitHubImageRows === undefined) {
        populateImages();
    }
    var runner = getRunner(image, gitHubImageRows);
    result = `<a href="${runner.link}">${runner.title}</a>`;
    return result;
}

/**
 * Get the runner object with the supplied name, from the list
 * of runners on GitHub.
 * @param {string} name name of the runner e.g. ubuntu-latest
 * @param {Array} runners array of runner row populated from the GitHub runners readme
 * @returns new object containing name and link of the runner
 */
function getRunner(name, runners) {
    for (let i = 0; i < runners.length; i++) {
        var runner = runners[i].split('|');
        if (runner[2].indexOf(`\`${name}\``) >= 0) {
            return {
                title: runner[1].trim(),
                link: gitHubLinks[runner[3].trim()]
            };
        }
    }
    return undefined;
}

/**
 * Get the lines from the markdown table which describe the runners.
 * @param {string} imagesSection section of the markdown which contains the table of runners
 * @returns markdown table lines not including the headings
 */
function getImageRows(imagesSection) {
    var lines = imagesSection.split(/\r?\n/);
    var rows = [];
    for (let i = 0; i < lines.length; i++) {
        if (lines[i].indexOf('|') === 0) {
            rows[rows.length] = lines[i];
        }
    }
    return rows.slice(2);
}

/**
 * Get link addresses for each runners readme, keyed on the runner name (including square brackets).
 * @param {string} linksSection section of the markdown which contains the links
 * @returns object where keys = runner names, and values = links
 */
function getLinks(linksSection) {
    var lines = linksSection.split(/\r?\n/);
    var links = [];
    for (let i = 0; i < lines.length; i++) {
        if (lines[i].search(/\[.*\]:/) === 0) {
            var parts = lines[i].split(": ");
            links[parts[0]] = parts[1];
        }
    }
    return links;
}

/**
 * Get the section of markdown which contains the table of runner images.
 * @param {string} markdown full markdown file
 * @returns markdown section
 */
function getImagesSection(markdown) {
    var sections = markdown.split('#');
    for (let i = 0; i < sections.length; i++) {
        if (sections[i].indexOf(' Available Images') === 0) {
            return sections[i];
        }
    }
    return undefined;
}

/**
 * Get the section of markdown which contains the runner readme links.
 * @param {string} markdown full markdown file
 * @returns markdown section
 */
function getLinksSection(markdown) {
    var sections = markdown.split('#');
    for (let i = 0; i < sections.length; i++) {
        if (sections[i].indexOf(' Label scheme') === 0) {
            return sections[i];
        }
    }
    return undefined;
}

/**
 * Populate the div with the id provided with a table containing the versions of OS etc.
 * which the repository provided is tested against.
 * The JSON file in 'ci/options.json' is used to populate, where the runner image is looked
 * up in GitHub runners, and the other dimensions are determined using functions passed as keys.
 * @param {string} project name of the repository
 * @param {string} divId id of the div to populate
 * @param {object} keys names and get functions for each testing dimension
 * e.g. { title: "Architecture", getValue: (d) => d.Arch }
 */
function grabTestedVersions(project, divId, keys) {
    var optionsUrl = `https://raw.githubusercontent.com/51Degrees/${project}/main/ci/options.json`
    $.getJSON(optionsUrl, function(options) {
        var platforms = {};

        for (let i = 0; i < options.length; i++) {
            var name = options[i].Image;
            if (Object.keys(platforms).indexOf(name) < 0) {
                platforms[name] = [];
            }
            if (hasConfig(platforms[name], options[i], keys) === false) {
                platforms[name][platforms[name].length] = options[i];
            }
        }
        var table = '<div class="g-table g-table--overflow"><table class="c-table">';
        table += '<tbody>';
        table += `<tr class="c-table__row--heading">${cell('Platform')}${cell('Tested Configurations')}</tr>`;
        for (const [platform, configs] of Object.entries(platforms)) {
            table += '<tr>';
            table += cell(getPlatform(platform));
            var list = '<ul>';
            for (let i = 0; i < configs.length; i++) {
                list += '<li>';
                for (let j = 0; j < keys.length; j++) {
                    list += `${keys[j].title}: ${keys[j].getValue(configs[i])} `;
                }
                list += '</li>';
            }
            list += '</ul>';
            table += cell(list);
            table += '</tr>';
        }
        table += '</tbody>';
        table += '</table></div>';
        var div = $('#' + divId);
        div.html(table);
    });
}

/**
 * Get an HTML table cell with the correct class.
 * @param {string} content string content to add in the cell
 * @returns HTML formatted cell
 */
function cell(content) {
    return `<td class="c-table__cell">${content}</td>`
}

/**
 * Determine whether or a list of configs contains a config already, using the
 * keys supplied to compare.
 * @param {Array} configs array to search
 * @param {object} config config to search for
 * @param {Array} keys array of keys to compare (see grabTestedVersions for structure)
 * @returns true if config is found
 */
function hasConfig(configs, config, keys) {
    for (let i = 0; i < configs.length; i++) {
        if (configsAreEqual(configs[i], config, keys)) {
            return true;
        }
    }
    return false;
}

/**
 * Determine whether two configs are equal, using the keys supplied to compare.
 * @param {object} first config to compare
 * @param {object} second other config to compare
 * @param {Array} keys array of keys to compare (see grabTestedVersions for structure)
 * @returns true if configs are equal
 */
function configsAreEqual(first, second, keys) {
    for (let i = 0; i < keys.length; i++) {
        var key = keys[i];
        if (key.getValue(first) != key.getValue(second)) {
            return false;
        }
    }
    return true;
}