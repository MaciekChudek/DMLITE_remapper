//gcc reader.c -o reader -lasound


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

void usage(char *name);
snd_seq_t *open_seq();
void midi_action(snd_seq_t *seq_handle);
int pedal_val;
int out_port;
int pedal_threshold = 64;

snd_seq_t *open_seq() {

	snd_seq_t *seq_handle;
	
	//open sequencer
	if (snd_seq_open(&seq_handle, "hw", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr, "Error opening ALSA sequencer.\n");
		exit(1);
	}
	
	//set sequencer name
	snd_seq_set_client_name(seq_handle, "DMLITE High-hat Remapper");
	
	//create input port
	if ((snd_seq_create_simple_port(seq_handle, "DMLITE High-hat Remapper",SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
		fprintf(stderr, "Error creating input port.\n");
		exit(1);
	}
	
	//create output port
	if ((out_port = snd_seq_create_simple_port(seq_handle, "DMLITE High-hat Remapper", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ, SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
			fprintf(stderr, "Error creating output port.\n");
			exit(1);
	}
	
	//return handle
	return(seq_handle);
}

void midi_action(snd_seq_t *seq_handle) {
	snd_seq_event_t *ev;
	do {
		//get event
		snd_seq_event_input(seq_handle, &ev);
		
		//set pedal value
		if(ev->type == SND_SEQ_EVENT_CONTROLLER && ev->data.control.channel == 9 && ev->data.control.param == 4){
			pedal_val = ev->data.control.value;
			fprintf(stderr, "Pedal... %5d\n", pedal_val);
		}
		//change high-hat signal
		if( (ev->type == SND_SEQ_EVENT_NOTEON || ev->type == SND_SEQ_EVENT_NOTEOFF) && ev->data.control.channel == 9 && ev->data.note.note == 46 && pedal_val >= pedal_threshold){
			ev->data.note.note = 42;
			fprintf(stderr, "Remapped... %5d\n", ev->data.note.note);
		}
		
		//resend event
		//snd_seq_ev_clear(ev);
		snd_seq_ev_set_source(ev, out_port );		
		snd_seq_ev_set_subs(ev);		
        snd_seq_ev_set_direct(ev);
        snd_seq_event_output_direct(seq_handle, ev);
		snd_seq_drain_output(seq_handle);
		snd_seq_free_event(ev);
		
	} while (snd_seq_event_input_pending(seq_handle, 0) > 0);
}



void usage(char *name){
	fprintf (stderr, "\
	Usage: %s [OPTIONS]\n\
	Remaps high-hat signal from DMLITE to 'closed high-hat' \n\
	if pedal is depressed more than t/127 of the way. \n\
	Add it to your ALSA midi chain with 'aconnect'. \n\
	\n\
		-h   This information \n\
		-t   Pedal threshold [64] \n\
	\n\
", name);
	exit(0);
}


int main(int argc, char *argv[]) {
	//parse arguments
	for (int i=1; i< argc; i++) {
	    if(argv[i][0] == '-'){
			if (argv[i][1] == 'h') usage(argv[0]);
	    	if (argv[i][1] == 't'){
				pedal_threshold = atoi(argv[i+1]);
				if(pedal_threshold > 127) pedal_threshold = 127;
				if(pedal_threshold < 0) pedal_threshold = 0;
			}
	    }
	}
	
	snd_seq_t *seq_handle;
	int npfd;
	struct pollfd *pfd;
	
	//open device
	seq_handle = open_seq();
	npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
	pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
	snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);
	//process midi stream
	while (1) {
		if (poll(pfd, npfd, 100000) > 0) {
			midi_action(seq_handle);
		}  
	}
}

