package edu.cs300;

import java.io.*;
import java.time.LocalDateTime;
import java.util.Map;
import java.util.Scanner;
import java.util.TreeMap;
import java.util.concurrent.ArrayBlockingQueue;

class Meeting {
    String description;
    String location;
    String datetime;
    int duration;

    public Meeting(String description, String location, String datetime, int duration) {
        this.description = description;
        this.location = location;
        this.datetime = datetime;
        this.duration = duration;
    }
}

class Worker extends Thread {

    ArrayBlockingQueue<MeetingRequest> incomingRequests;
    ArrayBlockingQueue<MeetingResponse> outgoingResponse;
    String empId;
    String empCalendar;

    TreeMap<LocalDateTime, Meeting> map = new TreeMap<LocalDateTime, Meeting>();

    public Worker(String empId, ArrayBlockingQueue<MeetingRequest> incomingRequests, ArrayBlockingQueue<MeetingResponse> outgoingResponse, String empCalendar) {
        this.incomingRequests = incomingRequests;
        this.outgoingResponse = outgoingResponse;
        this.empId = empId;
        this.empCalendar = empCalendar;

        File calendar = new File(this.empCalendar);
        try {
            Scanner cal = new Scanner(calendar);
            while (cal.hasNextLine()) {
                String line = cal.nextLine();
                String[] values = line.split(",");
                LocalDateTime time = LocalDateTime.parse(values[2]);
                map.put(time, new Meeting(values[0], values[1], values[2], Integer.parseInt(values[3])));
            }

        } catch (FileNotFoundException e) {
            System.err.println(e.getMessage());
        }
    }

    public void run() {
        DebugLog.log(" Thread (" + this.empId + ") thread started ...");
        while (true) {
            boolean valid = true;
            try {
                MeetingRequest mtgReq = (MeetingRequest) this.incomingRequests.take();
                if (mtgReq.request_id == 0) {
                    try {
                        File f = new File(empCalendar + ".bak");
                        FileWriter fr = new FileWriter(f, true);
                        BufferedWriter br = new BufferedWriter(fr);
                        for (Map.Entry<LocalDateTime, Meeting> entry : map.entrySet()) {
                            br.write(entry.getValue().description + "," + entry.getValue().location + "," + entry.getValue().datetime + "," + entry.getValue().duration + "\n");
                        }
                        br.close();
                        fr.close();
                        break;
                    } catch (IOException e) {
                        System.err.println(e.getMessage());
                    }

                }

                LocalDateTime time = LocalDateTime.parse(mtgReq.datetime);
                if (map.containsKey(time)) {
                    valid = false;
                } else {
                    if (map.floorKey(time) != null) {
                        if (map.floorKey(time).plusMinutes(map.floorEntry(time).getValue().duration).isAfter(time)) {
                            valid = false;
                        }
                    }
                    if (map.ceilingKey(time) != null) {
                        if (time.plusMinutes(mtgReq.duration).isAfter(map.ceilingKey(time).plusMinutes(map.ceilingEntry(time).getValue().duration))) {
                            valid = false;
                        }
                    }
                }
                if (valid) {
                    DebugLog.log("Worker-" + this.empId + " " + mtgReq + " pushing response " + mtgReq.request_id);
                    map.put(time, new Meeting(mtgReq.description, mtgReq.location, mtgReq.datetime, mtgReq.duration));
                    this.outgoingResponse.put(new MeetingResponse(mtgReq.request_id, 1));
                } else {
                    this.outgoingResponse.put(new MeetingResponse(mtgReq.request_id, 0));
                }

            } catch (InterruptedException e) {
                System.err.println(e.getMessage());
            }
        }
    }

}
