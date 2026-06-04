(function () {
  window.plausible =
    window.plausible ||
    function () {
      (window.plausible.q = window.plausible.q || []).push(arguments);
    };

  window.plausible.init =
    window.plausible.init ||
    function (options) {
      window.plausible.o = options || {};
    };

  window.plausible.init();

  var plausibleScript = document.createElement("script");
  plausibleScript.async = true;
  plausibleScript.src = "https://plausible.io/js/pa-_bHA8JlagL4VXKah7abto.js";
  document.head.appendChild(plausibleScript);

  var umamiScript = document.createElement("script");
  umamiScript.async = true;
  umamiScript.dataset.websiteId = "507b1cd1-2e02-48c9-a650-b8a8261e3d8b";
  umamiScript.src = "https://cloud.umami.is/script.js";
  document.head.appendChild(umamiScript);

  var cloudflareScript = document.createElement("script");
  cloudflareScript.defer = true;
  cloudflareScript.src = "https://static.cloudflareinsights.com/beacon.min.js";
  cloudflareScript.dataset.cfBeacon =
    '{"token": "796ca5d926a84b2b8c959e636b6ab0df"}';
  document.head.appendChild(cloudflareScript);

  var goatCounterScript = document.createElement("script");
  goatCounterScript.async = true;
  goatCounterScript.dataset.goatcounter =
    "https://mapequation.goatcounter.com/count";
  goatCounterScript.src = "//gc.zgo.at/count.js";
  document.head.appendChild(goatCounterScript);
})();
