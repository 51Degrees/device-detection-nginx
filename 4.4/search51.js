'use strict';

var onSearchTimer;
function onSearch(e,k) {
    if (k === 13)
    {
        window.location.href = "/SearchResults/" + e.value;
    }
    else {
        if (onSearchTimer != null) {
            clearTimeout(onSearchTimer);
        }
        onSearchTimer = setTimeout(function(){
            var searchTerm = e.value;
            fetch("/api/search/" + searchTerm, {
                cache: "no-cache"
            }).then((response) => {
                try {
                    return response.json();
                }
                finally {}
            }).then((result) => {
                if (result.length > 0 &&
                    searchTerm == e.value) {
                    var searchContent = document.getElementById("searchContent");
                    var searchOpenable = document.getElementById("searchOpenable");
                    searchContent.innerHTML = "";

                    var l = document.createElement("ul");
                    l.setAttribute("class", "c-search__list");
                    for (var i in result) {
                        var r = result[i];
                        var o = document.createElement("li");
                        o.setAttribute("class", "c-search__item");
                        var a = document.createElement("a");
                        a.setAttribute("href", r.url);
                        
                        var h = document.createElement("h5");
                        h.innerHTML = r.htmlTitle;

                        var p = document.createElement("p");
                        p.innerHTML = r.htmlText;

                        a.appendChild(h);
                        a.appendChild(p);
                        o.appendChild(a);
                        l.appendChild(o);
                    }
                    searchContent.appendChild(l);
                    searchOpenable.setAttribute("class", "c-search is-openable is-opened")
                }
            });
        }, 300);
    }
}