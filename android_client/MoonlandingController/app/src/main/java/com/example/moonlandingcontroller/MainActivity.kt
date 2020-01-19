package com.example.moonlandingcontroller

import android.annotation.SuppressLint
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.widget.*
import java.net.InetAddress
import java.net.Socket
import kotlin.concurrent.thread

const val ENGINE_BOTTOM: Int = 0

const val CODE_ENGINE_BOTTOM_ON: Int = 0b0000_0001
const val CODE_ENGINE_LEFT_ON: Int = 0b0000_0010
const val CODE_ENGINE_RIGHT_ON: Int = 0b0000_0100
const val CODE_ENGINE_TOP_ON: Int = 0b0000_1000

const val CODE_ENGINE_BOTTOM_OFF: Int = 0b0001_0000
const val CODE_ENGINE_LEFT_OFF: Int = 0b0010_0000
const val CODE_ENGINE_RIGHT_OFF: Int = 0b0100_0000
const val CODE_ENGINE_TOP_OFF: Int = 0b1000_0000

const val PORT: Int = 8888

class MainActivity : AppCompatActivity(), AdapterView.OnItemSelectedListener {

    var selectedEngine: Int = ENGINE_BOTTOM
    val ENGINE_ON_CODES = intArrayOf(CODE_ENGINE_BOTTOM_ON, CODE_ENGINE_LEFT_ON, CODE_ENGINE_RIGHT_ON, CODE_ENGINE_TOP_ON)
    val ENGINE_OFF_CODES = intArrayOf(CODE_ENGINE_BOTTOM_OFF, CODE_ENGINE_LEFT_OFF, CODE_ENGINE_RIGHT_OFF, CODE_ENGINE_TOP_OFF)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val engage_button: Button = findViewById(R.id.engine_button);
        val ip_text: TextView = findViewById(R.id.server_ip_text)
        val ip_addess = ip_text.text.trim().split('.').map { it.toInt().toByte() }.toByteArray()
        engage_button.setOnTouchListener { v, event ->
            if (event.action == MotionEvent.ACTION_DOWN) {
                sendCommand(ENGINE_ON_CODES[selectedEngine], ip_addess)
                true
            } else if (event.action == MotionEvent.ACTION_UP) {
                sendCommand(ENGINE_OFF_CODES[selectedEngine], ip_addess)
                true
            } else {
                false
            }
        }

        val spinner: Spinner = findViewById(R.id.engines_spinner)
        ArrayAdapter.createFromResource(
            this,
            R.array.engines_list,
            android.R.layout.simple_spinner_item
        ).also { adapter ->
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
            spinner.adapter = adapter
        }

        spinner.onItemSelectedListener = this
    }

    fun sendCommand(command: Int, ip_addess: ByteArray) {
        thread {
            val addr = InetAddress.getByAddress(ip_addess)
            val socket = Socket(addr, PORT)
            socket.getOutputStream().write(command)
        }
    }

    override fun onNothingSelected(parent: AdapterView<*>?) {
        selectedEngine = ENGINE_BOTTOM
    }

    override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
        selectedEngine = position
    }
}
