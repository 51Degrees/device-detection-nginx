var DOC_URL_BASE = 'documentation/';

/**
 * Get the documentation version from the URL.
 * @return the version string e.g. '4.0'.
 */
function getVersion() {
    var url = document.URL;
    return url.substring(url.indexOf(DOC_URL_BASE) + DOC_URL_BASE.length).split('/')[0];
}

/**
 * Update all the links in the example as they are relative.
 * @param project the repository name which the example came from
 * e.g. 'device-detection-cxx'.
 */
function updateLinks(project, divId) {
    var base = DOC_URL_BASE.split('/')[0];
    var as = $('#' + divId + ' a');
    for (i = 0; i < as.length; i++) {
        if (as[i].href.includes('/' + base + '/')) {
            // Replace the local part of the URL with the correct repository part
            // e.g. replace '/documentation/' with '/device-detection-cxx/'.
            as[i].href = as[i].href.replace('/' + base + '/', '/' + project + '/');
        }
    }
}

function addLink(url, divId) {
    var div = $('#' + divId);
    var link = "<div id=\"grabbed-example-link\" style=\"text-align:right;\"><a href=\"" + url + "\" class=\"b-link--dotted\">Go to original page...</a></div>";
    div.html(link + div.html());
}

function selectAllWithName(name, index) {
    var btns = document.getElementsByClassName("b-btn--tab");
    for (var i = 0; i < btns.length; i++) {
        if (btns[i].innerText === name) {
            btns[i].click();
        }
    }
} 

function selectFromMemory() {
    var selectedTabs = [];
    var selectedTabsString = readCookie('selectedTabs');
    if (selectedTabsString !== null) {
        selectedTabs = selectedTabsString.split(",");
    }
    selectedTabs.forEach(selectAllWithName);
}

$(document).ready(function() { selectFromMemory(); });

/**
 * Grab the example from the target language project, parse, and place in
 * the 'grabbed-example' div.
 * @param caller the button which called the function.
 * @param project the repository name to get the example from
 * e.g. 'device-detection-cxx'.
 * @param name the name of the example to get e.g. '_hash_2_getting_started_8cpp'.
 */
function grabExample(caller, project, name) {
    grabSnippet(
        caller,
        project,
        name + '-example.html',
        'primary',
        'examplebtn',
        'grabbed-example');
}

function grabSnippet(caller, project, file, tag, btnClass, divId) {
    selectBtn(caller, caller.parentElement.children);
    var url = '../../' + project + '/' + getVersion()
        + '/' + file;
    // Load the example into the 'grabbed-example' div, then update the links.
	var element = document.getElementById(divId);
	element.style = "display:block"
    $('#' + divId)
    .html('')
    .load(url + ' #' + tag, function() {
        updateLinks(project, divId);
        addLink(url, divId);
    });	
}

function selectBtn(caller, btns) {
    var selectedTabs = [];
    var selectedTabsString = readCookie('selectedTabs');
    if (selectedTabsString !== null) {
        selectedTabs = selectedTabsString.split(",");
    }
    for (i = 0; i < btns.length; i++) {
        if (btns[i] === caller) {
            // This is the selected button, so highlight it.
            if (!btns[i].classList.contains('b-btn--tab--is-active')) {
                btns[i].classList.add('b-btn--tab--is-active');
            }
            if (selectedTabs.indexOf(btns[i].innerText) < 0) {
                selectedTabs.push(btns[i].innerText);
            }
        }
        else {
            if (btns[i].classList.contains('b-btn--tab--is-active')) {
                // This is not the selected button, and it is highlighted,
                // so un highlight it.
                btns[i].classList.remove('b-btn--tab--is-active');
            }
            if (selectedTabs.indexOf(btns[i].innerText) >= 0) {
                selectedTabs.splice(selectedTabs.indexOf(btns[i].innerText), 1);
            }
			if(btns[i].classList.contains("c-tabgroup__main") && btns[i].getAttribute("data-lang") !== null){
				btns[i].style.display = "none"
			}
        }
    }
    document.cookie = "selectedTabs=" + selectedTabs.toString();
}

function showSnippet(caller, language) {
    selectBtn(caller, caller.parentElement.children);
    var div = caller.parentElement;
    for (i = 0; i < div.children.length; i++) {
        var item = div.children.item(i);
        if (item.classList.contains("c-tabgroup__main")) {
            if (item.getAttribute("data-lang") === language) {
                item.style.display = "block";
            }
            else {
                item.style.display = "none";
            }
        }
    }
}


function readCookie(name) {
    var nameEQ = name + "=";
    var ca = document.cookie.split(';');
    for (var i = 0; i < ca.length; i++) {
        var c = ca[i];
        while (c.charAt(0) == ' ') c = c.substring(1, c.length);
        if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length, c.length);
    }
    return "";
}