let aggiornaclock;

$(function(){

	// Cache some selectors

	var clock = $('#clock');
		//alarm = clock.find('.alarm'),
		//ampm = clock.find('.ampm');

	// Map digits to their names (this will be an array)
	var digit_to_name = 'zero one two three four five six seven eight nine'.split(' ');

	// This object will hold the digit elements
	var digitsdata = {};
	var digitstime = {};

	// Positions for the hours, minutes, and seconds
	var positions = [
		'h1', 'h2', ':', 'm1', 'm2', ':', 's1', 's2'
	];
	var positionsdata = [
		'd1', 'd2', ':', 'm1', 'm2', ':', 'a1', 'a2','a3', 'a4'
	];

	// Generate the digits with the needed markup,
	// and add them to the clock
	
	//var digit_holder = clock.find('.digits');
	var digit_holder = $('#display').find('.digits');

	$.each(positions, function(){

		if(this == ':'){
			digit_holder.append('<div class="dots">');
		}
		else{

			var pos = $('<div>');

			for(var i=1; i<8; i++){
				pos.append('<span class="d' + i + '">');
			}

			// Set the digits as key:value pairs in the digits object
			digitsdata[this] = pos;

			// Add the digit elements to the page
			digit_holder.append(pos);
		}

	});

	var digit_holder = $('#display2').find('.digits');

	$.each(positionsdata, function(){

		if(this == ':'){
			digit_holder.append('<div class="dots">');
		}
		else{

			var pos = $('<div>');

			for(var i=1; i<8; i++){
				pos.append('<span class="d' + i + '">');
			}

			// Set the digits as key:value pairs in the digits object
			digitstime[this] = pos;

			// Add the digit elements to the page
			digit_holder.append(pos);
		}

	});


	
	// Add the weekday names

	var weekday_names = 'LUN MAR MER GIO VEN SAB DOM'.split(' '),
		weekday_holder = clock.find('.weekdays');

	$.each(weekday_names, function(){
		weekday_holder.append('<span>' + this + '</span>');
	});

	var weekdays = clock.find('.weekdays span');


	// Run a timer every second and update the clock

		(aggiornaclock=  function update_time(){

		// Use moment.js to output the current time as a string
		// hh is for the hours in 12-hour format,
		// mm - minutes, ss-seconds (all with leading zeroes),
		// d is for day of week and A is for AM/PM

		//var now = moment().format("hhmmssdA");
		
		digitsdata.h1.attr('class', digit_to_name[dataricevuta[12]]);
		digitsdata.h2.attr('class', digit_to_name[dataricevuta[13]]);
		digitsdata.m1.attr('class', digit_to_name[dataricevuta[15]]);
		digitsdata.m2.attr('class', digit_to_name[dataricevuta[16]]);
		digitsdata.s1.attr('class', digit_to_name[dataricevuta[18]]);
		digitsdata.s2.attr('class', digit_to_name[dataricevuta[19]]);

		// The library returns Sunday as the first day of the week.
		// Stupid, I know. Lets shift all the days one position down, 
		// and make Sunday last

		var dow = dataricevuta[21];
		dow--;
		
		// Sunday!
		if(dow < 0){
			// Make it last
			dow = 6;
		}

		// Mark the active day of the week
		weekdays.removeClass('active').eq(dow).addClass('active');

		// Set the am/pm text:
		//ampm.text(now[7]+now[8]);


		var data ='12072022';
		digitstime.d1.attr('class', digit_to_name[dataricevuta[8]]);
		digitstime.d2.attr('class', digit_to_name[dataricevuta[9]]);
		digitstime.m1.attr('class', digit_to_name[dataricevuta[5]]);
		digitstime.m2.attr('class', digit_to_name[dataricevuta[6]]);
		digitstime.a1.attr('class', digit_to_name[dataricevuta[0]]);
		digitstime.a2.attr('class', digit_to_name[dataricevuta[1]]);
		digitstime.a3.attr('class', digit_to_name[dataricevuta[2]]);
		digitstime.a4.attr('class', digit_to_name[dataricevuta[3]]);

		// Schedule this function to be run again in 1 sec
		//setTimeout(update_time, 1000);

	})();

	// Switch the theme
	/*
	$('a.button').click(function(){
		clock.toggleClass('light dark');
	});
	*/
});