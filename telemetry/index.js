const profs = require('./t2Profiles');

//
// MAIN
//
try {
   const profiles = new profs.Profiles("profiles");
   profiles.processProfiles();
} catch (e) {
  console.log(e);
}

